/* Common FPS for PS5 - Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawn} = require('node:child_process');
const {once} = require('node:events');
const {chromium} = require('playwright');

(async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'commonfps-ui-'));
  const config = path.join(dir, 'config.ini');
  let child, browser;
  async function start() {
    const proc = spawn(process.argv[2], [config], {stdio:['ignore','pipe','inherit']});
    await Promise.race([
      once(proc.stdout,'data'),
      once(proc,'exit').then(([code])=>{throw new Error('Settings fixture exited '+code);})
    ]);
    return proc;
  }
  async function stop() { const done = once(child,'exit'); child.kill(); await done; child = null; }
  try {
    child = await start();
    browser = await chromium.launch({headless:true,args:['--no-sandbox']});
    const page = await browser.newPage({viewport:{width:1920,height:1080}});
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    const url = 'http://127.0.0.1:39029/';
    await page.goto(url);
    await page.getByRole('status').filter({hasText:'Настройки загружены'}).waitFor();
    assert.equal(await page.locator('#font_size').textContent(), '22');
    await page.locator('[data-corner="top_right"]').click();
    await page.locator('[data-step="font_size:2"]').click();
    await page.locator('#toggle').click();
    const initialColor = await page.locator('#value_color').inputValue();
    await page.locator('[data-color="value_color"]').click();
    await page.getByRole('dialog').waitFor();
    assert.equal(await page.locator('#palette-grid button').count(),8);
    assert.equal(await page.locator('#value_color').inputValue(),initialColor); // opening is not a color change
    await page.locator('#palette-grid [data-rgb="#B366FF"]').click();
    assert.equal(await page.getByRole('dialog').isVisible(),false);
    await page.locator('#label_color').fill('#123ABC');
    await page.locator('#save').click();
    await page.getByRole('status').filter({hasText:'Сохранено.'}).waitFor();
    let text = fs.readFileSync(config,'utf8');
    assert(text.includes('corner=top_right\n') && text.includes('font_size=24\n'));
    assert(text.includes('show_label=0\n') && text.includes('value_color=#B366FF\n'));
    assert(text.includes('label_color=#123ABC\n'));
    assert.equal(await page.locator('#sample-label').isVisible(), false);
    // Restart the actual server process, then reload the browser.
    await stop(); child = await start();
    await page.reload();
    await page.getByRole('status').filter({hasText:'Настройки загружены'}).waitFor();
    assert.equal(await page.locator('[data-corner="top_right"]').getAttribute('aria-pressed'),'true');
    assert.equal(await page.locator('#font_size').textContent(),'24');
    assert.equal(await page.locator('#label_color').inputValue(),'#123ABC');
    // Invalid color must not overwrite a valid stored configuration.
    await page.locator('#value_color').fill('#GGGGGG');
    await page.locator('#save').click();
    assert.equal(fs.readFileSync(config,'utf8'),text);
    // Recover from invalid custom text without silently changing disk state.
    await page.locator('#value_color').fill('#B366FF');
    await page.locator('#label_color').focus();
    await page.locator('#value_color').blur();
    const posts = [];
    page.on('request',request=>{if(request.method()==='POST' && request.url().endsWith('/api/config'))posts.push(request.postData());});
    await page.addInitScript(() => {
      window.__pad = null;
      Object.defineProperty(navigator,'getGamepads',{value:()=>window.__pad?[window.__pad]:[]});
    });
    await page.evaluate(() => {
      window.__pad = null;
      Object.defineProperty(navigator,'getGamepads',{value:()=>window.__pad?[window.__pad]:[]});
    });
    async function padPress(index,numeric=false,hold=80) {
      await page.evaluate(({index,numeric})=>{window.__pad.buttons[index]=numeric?1:{pressed:true,value:1};},{index,numeric});
      await page.waitForTimeout(hold);
      await page.evaluate(({index,numeric})=>{window.__pad.buttons[index]=numeric?0:{pressed:false,value:0};},{index,numeric});
      await page.waitForTimeout(80);
    }
    async function setPad(numeric=false,rawSony=false) {
      await page.evaluate(({numeric,rawSony})=>{
        window.__pad={id:rawSony?'Wireless Controller (Vendor: 054c Product: 09cc)':'Controller',mapping:rawSony?'':'standard',connected:true,axes:[0,0],buttons:Array.from({length:17},()=>numeric?0:{pressed:false,value:0})};
      },{numeric,rawSony});
      await page.waitForTimeout(80);
    }
    async function focusSaveWithDpad() {
      await page.locator('[data-color="label_color"]').focus();
      await padPress(15); // right, using the actual polling loop
      assert.equal(await page.evaluate(()=>document.activeElement.id),'save');
    }
    // Standard GamepadButton objects: D-pad focuses Save; Cross persists to disk.
    await page.waitForTimeout(350);
    await setPad();
    await focusSaveWithDpad();
    let count=posts.length;
    await padPress(0,false,600); // held Cross still emits exactly one save
    await page.getByRole('status').filter({hasText:'Сохранено.'}).waitFor();
    assert.equal(posts.length,count+1);
    // Legacy numeric Gamepad API used by older WebKit variants.
    await page.waitForTimeout(350);
    await setPad(true);
    await focusSaveWithDpad(); count=posts.length;
    await padPress(0,true);
    await page.getByRole('status').filter({hasText:'Сохранено.'}).waitFor();
    assert.equal(posts.length,count+1);
    // Raw Sony face-button order: Cross is button 1, not Circle.
    await page.waitForTimeout(350);
    await setPad(false,true);
    await page.locator('#save').focus(); count=posts.length;
    await padPress(1);
    await page.getByRole('status').filter({hasText:'Сохранено.'}).waitFor();
    assert.equal(posts.length,count+1);
    // Legacy keyCode without KeyboardEvent.key must activate Save too.
    await page.waitForTimeout(350);
    count=posts.length;
    await page.evaluate(()=>{
      document.querySelector('#save').focus();
      document.dispatchEvent(new KeyboardEvent('keydown',{key:'',keyCode:13,bubbles:true,cancelable:true}));
      document.dispatchEvent(new KeyboardEvent('keyup',{key:'',keyCode:13,bubbles:true,cancelable:true}));
    });
    await page.getByRole('status').filter({hasText:'Сохранено.'}).waitFor();
    assert.equal(posts.length,count+1);
    // A native click at the stationary cursor must honor D-pad focus.
    const pointerButton=await page.locator('[data-corner="top_left"]').boundingBox();
    await page.mouse.move(pointerButton.x+10,pointerButton.y+10);
    await page.waitForTimeout(350);
    await page.locator('[data-color="label_color"]').focus();
    await page.keyboard.press('ArrowRight');
    assert.equal(await page.evaluate(()=>document.activeElement.id),'save');
    count=posts.length;
    await page.mouse.down(); await page.mouse.up();
    await page.getByRole('status').filter({hasText:'Сохранено.'}).waitFor();
    assert.equal(posts.length,count+1);
    assert(fs.readFileSync(config,'utf8').includes('corner=top_right\n'));
    // One physical Cross delivered as gamepad + Enter + native click is one action.
    await page.waitForTimeout(350);
    await setPad(); await page.locator('#save').focus(); count=posts.length;
    await page.evaluate(()=>{window.__pad.buttons[0]={pressed:true,value:1};});
    await page.waitForTimeout(50);
    await page.keyboard.press('Enter'); await page.mouse.down(); await page.mouse.up();
    await page.evaluate(()=>{window.__pad.buttons[0]={pressed:false,value:0};});
    await page.waitForTimeout(150);
    assert.equal(posts.length,count+1);
    // Selecting a swatch with Cross, canceling a palette, and no hidden HEX traps.
    await page.waitForTimeout(350);
    await page.locator('[data-color="label_color"]').focus();
    await padPress(0); await page.getByRole('dialog').waitFor();
    await padPress(15); // next explicit swatch, green after blue/custom default fallback
    const rgb=await page.evaluate(()=>document.activeElement.dataset.rgb);
    await padPress(0);
    assert.equal(await page.getByRole('dialog').isVisible(),false);
    assert.equal(await page.locator('#label_color').inputValue(),rgb);
    await padPress(0); await page.getByRole('dialog').waitFor();
    await padPress(1);
    assert.equal(await page.getByRole('dialog').isVisible(),false);
    assert.equal(await page.locator('#label_color').inputValue(),rgb);
    await padPress(13);
    assert.equal(await page.evaluate(()=>document.activeElement.tagName),'BUTTON');
    // OPTIONS saves without depending on pointer position.
    count=posts.length; await padPress(9);
    await page.getByRole('status').filter({hasText:'Сохранено.'}).waitFor();
    assert.equal(posts.length,count+1);
    // Mouse remains usable after controller navigation.
    await page.mouse.move(20,20);
    await page.waitForTimeout(350);
    text=fs.readFileSync(config,'utf8');
    await page.locator('#reset').click();
    assert.equal(fs.readFileSync(config,'utf8'),text); // reset needs Save
    await page.locator('#save').click();
    await page.getByRole('status').filter({hasText:'Сохранено.'}).waitFor();
    assert(fs.readFileSync(config,'utf8').includes('font_size=22\n'));
    for (const [width,height] of [[1920,1080],[1280,720],[800,1100]]) {
      await page.setViewportSize({width,height});
      assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
      const sample = await page.locator('#sample').boundingBox();
      const preview = await page.locator('#preview').boundingBox();
      assert(sample.x>=preview.x && sample.y>=preview.y);
      assert(sample.x+sample.width<=preview.x+preview.width);
      assert(sample.y+sample.height<=preview.y+preview.height);
    }
    await page.setViewportSize({width:1920,height:1080});
    const screenshot = process.argv[3];
    if (screenshot) await page.screenshot({path:screenshot,fullPage:true});
    assert.deepEqual(errors,[]);
    console.log('Real HTTP worker + browser: controls, save, process restart, palette, standard/raw/legacy Gamepad, mixed-event deduplication, keyboard, viewport PASS');
  } finally {
    if (browser) await browser.close();
    if (child) await stop();
    fs.rmSync(dir,{recursive:true,force:true});
  }
})().catch(error=>{console.error(error);process.exitCode=1;});
