/* Common FPS v0.28b SR9F - enqueue execution proof controller */
#include "process_sysctl.hpp"
#include "stable_injector.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

extern "C" {
extern const unsigned char commonfps_sr9f_receiver_elf[];
extern const std::size_t commonfps_sr9f_receiver_elf_size;
int commonfps_v028b_trace_continue_seen(void);
int commonfps_v028b_trace_stop_seen(void);
unsigned commonfps_v028b_import_resolved_count(void);
unsigned commonfps_v028b_import_unresolved_count(void);
const char* commonfps_v028b_first_unresolved(void);
}

namespace {
constexpr const char* kLogPath = "/data/CommonFPS_SR9F_enqueue_execution.log";
constexpr std::uint16_t kPortNetwork = 0xFAD8U;
constexpr std::uint32_t kLoopback = 0x0100007FU;
constexpr std::uint32_t kMagic = 0x46394551U;
struct ResultPacket { std::uint32_t magic, kind; std::uint64_t sequence; std::int32_t value; std::uint32_t code; char detail[96]; char extra[160]; };
void log_line(const char* s){if(FILE* f=std::fopen(kLogPath,"a")){std::fprintf(f,"%s\n",s);std::fclose(f);}}
void log_pid(const char* t,pid_t p){if(FILE* f=std::fopen(kLogPath,"a")){std::fprintf(f,"%s%d\n",t,(int)p);std::fclose(f);}}
void log_injection(const common_fps::legacy_v028b::InjectionResult& r){if(FILE* f=std::fopen(kLogPath,"a")){const char* u=commonfps_v028b_first_unresolved();std::fprintf(f,"SR9F INJECT attached=%d elf_loaded=%d payload_args=%d bootstrap=%d pthread_ok=%d pthread_rc=%d trace_continue=%d trace_stop=%d imports_resolved=%u imports_unresolved=%u first_unresolved=%s\n",r.attached?1:0,r.elf_loaded?1:0,r.payload_args_ready?1:0,r.bootstrap_started?1:0,r.pthread_create_ok?1:0,r.pthread_create_rc,commonfps_v028b_trace_continue_seen(),commonfps_v028b_trace_stop_seen(),commonfps_v028b_import_resolved_count(),commonfps_v028b_import_unresolved_count(),(u&&*u)?u:"none");std::fclose(f);}}
void log_packet(const ResultPacket& p){if(FILE* f=std::fopen(kLogPath,"a")){if(p.kind==1)std::fprintf(f,"SR9F STAGE seq=%llu detail=%s extra=%s\n",(unsigned long long)p.sequence,p.detail,p.extra);else if(p.kind==2)std::fprintf(f,"SR9F RESULT value=%d detail=%s extra=%s\n",p.value,p.detail,p.extra);else if(p.kind==3)std::fprintf(f,"SR9F DONE detail=%s extra=%s\n",p.detail,p.extra);else if(p.kind==4)std::fprintf(f,"SR9F ERROR code=%u detail=%s extra=%s\n",p.code,p.detail,p.extra);std::fclose(f);}}
pid_t wait_shellui(){using common_fps::legacy_v028b::find_process_pid_sysctl; pid_t prev=-1; for(unsigned i=0;i<8;++i){pid_t n=find_process_pid_sysctl("SceShellUI"); if(n>0&&n==prev)return n; prev=n; sleep(2);} return -1;}
int run_probe(){using common_fps::legacy_v028b::inject_renderer_once; log_pid("SR9F CHILD pid=",getpid()); log_line("SR9F START PUI EnqueueEventAction execution proof"); log_line("SR9F NO CreateLabel / NO AppendChild / NO PUI mutation / NO Detour / NO code patch"); log_line("SR9F proof: managed ArrayList Count 1 -> queued Action Clear -> Count 0"); int s=socket(AF_INET,SOCK_DGRAM,0); if(s<0)return 1; int reuse=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)); sockaddr_in a{}; a.sin_family=AF_INET;a.sin_port=kPortNetwork;a.sin_addr.s_addr=kLoopback; if(bind(s,(sockaddr*)&a,sizeof(a))!=0){close(s);return 2;} pid_t sh=wait_shellui(); log_pid("SR9F ShellUI stable pid=",sh); if(sh<=0){close(s);return 3;} auto inj=inject_renderer_once(sh,commonfps_sr9f_receiver_elf,commonfps_sr9f_receiver_elf_size); log_injection(inj); if(commonfps_v028b_import_unresolved_count()!=0||!inj.pthread_create_ok){close(s);return 4;} bool done=false,err=false; for(unsigned ms=0;ms<10000;ms+=10){ResultPacket p{}; ssize_t n=recvfrom(s,&p,sizeof(p),MSG_DONTWAIT,nullptr,nullptr); if(n==(ssize_t)sizeof(p)&&p.magic==kMagic){log_packet(p); if(p.kind==3){done=true;break;} if(p.kind==4){err=true;break;}} usleep(10000);} close(s); if(err){log_line("SR9F FAIL receiver reported enqueue/execution error");return 5;} if(!done){log_line("SR9F FAIL enqueue execution timeout");return 6;} log_line("SR9F PASS worker -> PUI queue -> Action execution proven; no UI mutation"); return 0;}
}
extern "C" int main(){log_pid("SR9F PARENT start pid=",getpid()); pid_t c=fork(); if(c>0){log_pid("SR9F PARENT forked child=",c);log_line("SR9F PARENT RETURN 0");return 0;} if(c<0)log_line("SR9F fork failed; run current process"); return run_probe();}
