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
    await page.locator('[data-color="value_color"]').click();
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
    // Native keyboard focus can reach the next corner and activate it.
    await page.locator('[data-corner="top_left"]').focus();
    await page.keyboard.press('ArrowRight');
    assert.equal(await page.evaluate(()=>document.activeElement.dataset.corner),'top_right');
    await page.keyboard.press('Enter');
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
    console.log('Real HTTP worker + browser: controls, save, process restart, colors, keyboard, viewport PASS');
  } finally {
    if (browser) await browser.close();
    if (child) await stop();
    fs.rmSync(dir,{recursive:true,force:true});
  }
})().catch(error=>{console.error(error);process.exitCode=1;});
