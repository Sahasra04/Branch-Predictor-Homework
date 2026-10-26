#include <cassert>
#include <cstdio>
#include <cstring>
#include "branch.h"
#include "predictor.h"
#include "my_predictor.h"
int main(){
 pm_predictor p;
 branch_info b; b.address=0x1234;b.opcode=OP_JZ;b.br_flags=BR_CONDITIONAL;
 branch_update *u=p.predict(b);
 assert(!u->direction_prediction());
 assert(p.u.index==(0x1234u&4095u));
 p.update(u,true,0x2000);
 assert(p.bim[p.u.index]==2);
 assert(p.ghr==1);
 assert(p.global[p.u.set][0].valid);
 assert(p.global[p.u.set][0].ctr==2);
 u=p.predict(b);p.update(u,true,0x2000);
 assert(p.ghr==3);
 for(int i=0;i<512;i++) {
  bool used[4]={false,false,false,false};
  for(int j=0;j<4;j++){unsigned a=p.global[i][j].age;assert(a<4);assert(!used[a]);used[a]=true;}
 }
 unsigned prev=p.ghr;
 b.br_flags=BR_CALL;u=p.predict(b);assert(u->direction_prediction());p.update(u,true,0x2000);
 assert(p.ghr==prev);
 cpm_predictor x;
 b.br_flags=BR_CONDITIONAL;b.address=0x5678;
 u=x.predict(b);assert(!u->direction_prediction());
 x.update(u,false,b.address+6);
 u=x.predict(b);assert(u->target_prediction()==b.address+6);
 x.update(u,true,0x9000);
 u=x.predict(b);assert(x.btb[x.u.btb_set][x.u.btb_way].valid);
 assert(x.pir!=0);
 printf("PASS: main counters/history/allocation/LRU; unconditional behavior; extra-credit fallthrough/BTB/PIR\n");
 return 0;
}
