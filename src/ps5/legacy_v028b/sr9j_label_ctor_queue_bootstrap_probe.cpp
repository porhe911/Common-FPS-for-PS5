/* Common FPS v0.28b SR9J - queued Label constructor controller */
#include "process_sysctl.hpp"
#include "stable_injector.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

extern "C" { extern const unsigned char commonfps_sr9j_receiver_elf[]; extern const std::size_t commonfps_sr9j_receiver_elf_size; int commonfps_v028b_trace_continue_seen(void); int commonfps_v028b_trace_stop_seen(void); unsigned commonfps_v028b_import_resolved_count(void); unsigned commonfps_v028b_import_unresolved_count(void); const char* commonfps_v028b_first_unresolved(void); }
namespace {
constexpr const char* kLogPath="/data/CommonFPS_SR9J_label_ctor_queue.log"; constexpr std::uint16_t kPortNetwork=0xFDD8U; constexpr std::uint32_t kLoopback=0x0100007FU; constexpr std::uint32_t kMagic=0x4a39434cU;
struct P{std::uint32_t magic,kind;std::uint64_t seq;std::int32_t value;std::uint32_t code;char detail[96];char extra[160];};
void line(const char*s){if(FILE*f=std::fopen(kLogPath,"a")){std::fprintf(f,"%s\n",s);std::fclose(f);}} void pidline(const char*t,pid_t p){if(FILE*f=std::fopen(kLogPath,"a")){std::fprintf(f,"%s%d\n",t,(int)p);std::fclose(f);}}
void injlog(const common_fps::legacy_v028b::InjectionResult&r){if(FILE*f=std::fopen(kLogPath,"a")){const char*u=commonfps_v028b_first_unresolved();std::fprintf(f,"SR9J INJECT attached=%d elf_loaded=%d payload_args=%d bootstrap=%d pthread_ok=%d pthread_rc=%d trace_continue=%d trace_stop=%d imports_resolved=%u imports_unresolved=%u first_unresolved=%s\n",r.attached?1:0,r.elf_loaded?1:0,r.payload_args_ready?1:0,r.bootstrap_started?1:0,r.pthread_create_ok?1:0,r.pthread_create_rc,commonfps_v028b_trace_continue_seen(),commonfps_v028b_trace_stop_seen(),commonfps_v028b_import_resolved_count(),commonfps_v028b_import_unresolved_count(),(u&&*u)?u:"none");std::fclose(f);}}
void plog(const P&p){if(FILE*f=std::fopen(kLogPath,"a")){if(p.kind==1)std::fprintf(f,"SR9J STAGE seq=%llu detail=%s extra=%s\n",(unsigned long long)p.seq,p.detail,p.extra);else if(p.kind==2)std::fprintf(f,"SR9J RESULT value=%d detail=%s extra=%s\n",p.value,p.detail,p.extra);else if(p.kind==3)std::fprintf(f,"SR9J DONE detail=%s extra=%s\n",p.detail,p.extra);else if(p.kind==4)std::fprintf(f,"SR9J ERROR code=%u detail=%s extra=%s\n",p.code,p.detail,p.extra);std::fclose(f);}}
pid_t waitsh(){using common_fps::legacy_v028b::find_process_pid_sysctl;pid_t prev=-1;for(unsigned i=0;i<8;++i){pid_t n=find_process_pid_sysctl("SceShellUI");if(n>0&&n==prev)return n;prev=n;sleep(2);}return -1;}
int run(){using common_fps::legacy_v028b::inject_renderer_once;pidline("SR9J CHILD pid=",getpid());line("SR9J START queued Label constructor proof");line("SR9J no tree mutation, no Label properties, no font/color, no dynamic FPS");line("SR9J DO NOT launch a game; controller exits after one queued constructor chain");int s=socket(AF_INET,SOCK_DGRAM,0);if(s<0)return 1;int reuse=1;setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=kPortNetwork;a.sin_addr.s_addr=kLoopback;if(bind(s,(sockaddr*)&a,sizeof(a))!=0){close(s);return 2;}pid_t sh=waitsh();pidline("SR9J ShellUI stable pid=",sh);if(sh<=0){close(s);return 3;}auto r=inject_renderer_once(sh,commonfps_sr9j_receiver_elf,commonfps_sr9j_receiver_elf_size);injlog(r);if(commonfps_v028b_import_unresolved_count()!=0||!r.pthread_create_ok){close(s);return 4;}bool done=false,err=false;for(unsigned ms=0;ms<6000;ms+=10){P p{};ssize_t n=recvfrom(s,&p,sizeof(p),MSG_DONTWAIT,nullptr,nullptr);if(n==(ssize_t)sizeof(p)&&p.magic==kMagic){plog(p);if(p.kind==3){done=true;break;}if(p.kind==4){err=true;break;}}usleep(10000);}close(s);if(err){line("SR9J FAIL receiver reported queued Label constructor error");return 5;}if(!done){line("SR9J FAIL short action timeout");return 6;}line("SR9J PASS Label constructor executed through PUI queue before managed proof action");return 0;}
}
extern "C" int main(){pidline("SR9J PARENT start pid=",getpid());pid_t c=fork();if(c>0){pidline("SR9J PARENT forked child=",c);line("SR9J PARENT RETURN 0");return 0;}if(c<0)line("SR9J fork failed; run current process");return run();}
