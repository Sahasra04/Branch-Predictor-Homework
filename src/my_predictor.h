// my_predictor.h
// This file contains a sample gshare_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.

class gshare_update : public branch_update {
public:
	unsigned int index;
};

class gshare_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	15
#define TABLE_BITS	15
	gshare_update u;
	branch_info bi;
	unsigned int history;
	unsigned char tab[1<<TABLE_BITS];

	gshare_predictor (void) : history(0) { 
		memset (tab, 0, sizeof (tab));
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			u.index = 
				  (history << (TABLE_BITS - HISTORY_LENGTH)) 
				^ (b.address & ((1<<TABLE_BITS)-1));
			u.direction_prediction (tab[u.index] >> 1);
		} else {
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
			unsigned char *c = &tab[((gshare_update*)u)->index];
			if (taken) {
				if (*c < 3) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}
			history <<= 1;
			history |= taken;
			history &= (1<<HISTORY_LENGTH)-1;
		}
	}
};

//
// Pentium M hybrid branch predictors
// This class implements a simple hybrid branch predictor based on the Pentium M branch outcome prediction units. 
// Instead of implementing the complete Pentium M branch outcome predictors, the class below implements a hybrid 
// predictor that combines a bimodal predictor and a global predictor. 

// Pentium M-inspired 4-way tagged hybrid direction predictor.
// The course permits outcome history in place of the proprietary PIR hash.
class pm_update : public branch_update {
public:
    unsigned int index, set, tag, way;
    bool conditional, hit, bim_prediction;
};
class pm_predictor : public branch_predictor {
public:
    enum { BIM_SIZE=4096, SETS=512, WAYS=4 };
    struct entry { unsigned short tag; unsigned char ctr, age, valid; };
    pm_update u;
    unsigned char bim[BIM_SIZE];
    entry global[SETS][WAYS];
    unsigned int ghr;
    pm_predictor():ghr(0) {
        memset(bim,1,sizeof(bim));
        memset(global,0,sizeof(global));
        for(unsigned int i=0;i<SETS;i++)
            for(unsigned int j=0;j<WAYS;j++) global[i][j].age=j;
    }
    static void train(unsigned char &c,bool taken) {
        if(taken && c<3) ++c;
        if(!taken && c>0) --c;
    }
    void touch(unsigned int set,unsigned int way) {
        unsigned int old=global[set][way].age;
        for(unsigned int j=0;j<WAYS;j++)
            if(j!=way && global[set][j].age<old) ++global[set][j].age;
        global[set][way].age=0;
    }
    branch_update *predict(branch_info &b) {
        u.conditional=(b.br_flags & BR_CONDITIONAL)!=0;
        u.index=b.address & 4095;
        unsigned int hash=(b.address ^ ghr)&0x7fff;
        u.set=hash>>6;
        u.tag=hash&63;
        u.hit=false;u.way=0;
        u.bim_prediction=bim[u.index]>=2;
        bool prediction=u.bim_prediction;
        if(u.conditional) {
            for(unsigned int j=0;j<WAYS;j++) {
                entry &e=global[u.set][j];
                if(e.valid && e.tag==u.tag) {
                    u.hit=true;u.way=j;prediction=e.ctr>=2;break;
                }
            }
        } else prediction=true;
        u.direction_prediction(prediction);
        u.target_prediction(0);
        return &u;
    }
    void update(branch_update *p,bool taken,unsigned int target) {
        pm_update *r=static_cast<pm_update*>(p);
        if(!r->conditional) return;
        train(bim[r->index],taken);
        if(r->hit) {
            train(global[r->set][r->way].ctr,taken);
            touch(r->set,r->way);
        } else if(r->bim_prediction!=taken) {
            unsigned int victim=0;
            bool found=false;
            for(unsigned int j=0;j<WAYS;j++) if(!global[r->set][j].valid) {
                victim=j;found=true;break;
            }
            if(!found) for(unsigned int j=0;j<WAYS;j++)
                if(global[r->set][j].age==WAYS-1) {victim=j;break;}
            entry &e=global[r->set][victim];
            e.valid=1;e.tag=r->tag;e.ctr=taken?2:1;
            touch(r->set,victim);
        }
        ghr=((ghr<<1)|(taken?1u:0u))&0x7fff;
    }
};

// Extra credit: complete direction + target predictor approximation.
// Includes a 4-way BTB, 4-way indirect BTB, tagged loop predictor,
// and path-information register (PIR). Exact Intel proprietary
// replacement and hashing policies are not public.
class cpm_update : public branch_update {
public:
    branch_info branch;
    bool loop_hit, loop_prediction, used_loop;
    unsigned int loop_set, loop_tag, loop_way;
    unsigned int btb_set, btb_tag, btb_way;
    bool btb_hit;
    unsigned int ibtb_set, ibtb_tag, ibtb_way;
    bool ibtb_hit;
};
class cpm_predictor : public branch_predictor {
public:
    enum { BTB_SETS=512, IBTB_SETS=256, LOOP_SETS=64 };
    struct target_entry { unsigned int tag,target; unsigned char valid,age; };
    struct loop_entry {
        unsigned int tag;
        unsigned short count,limit;
        unsigned char valid,confidence,dir,age;
    };
    cpm_update u;
    pm_predictor direction;
    target_entry btb[BTB_SETS][4],ibtb[IBTB_SETS][4];
    struct fall_entry { unsigned int tag, target; bool valid; };
    fall_entry fall[4096];
    loop_entry loop[LOOP_SETS][2];
    unsigned int pir;
    cpm_predictor():pir(0) {
        memset(btb,0,sizeof(btb));memset(ibtb,0,sizeof(ibtb));
        memset(loop,0,sizeof(loop));memset(fall,0,sizeof(fall));
        for(unsigned int i=0;i<BTB_SETS;i++) for(int j=0;j<4;j++) btb[i][j].age=j;
        for(unsigned int i=0;i<IBTB_SETS;i++) for(int j=0;j<4;j++) ibtb[i][j].age=j;
        for(unsigned int i=0;i<LOOP_SETS;i++) for(int j=0;j<2;j++) loop[i][j].age=j;
    }
    template <typename E> static void touch(E *row,unsigned int n,unsigned int w) {
        unsigned int old=row[w].age;
        for(unsigned int j=0;j<n;j++)
            if(j!=w && row[j].age<old) ++row[j].age;
        row[w].age=0;
    }
    template <typename E> static unsigned int victim(E *row,unsigned int n) {
        for(unsigned int j=0;j<n;j++) if(!row[j].valid) return j;
        for(unsigned int j=0;j<n;j++) if(row[j].age==n-1) return j;
        return 0;
    }
    branch_update *predict(branch_info &b) {
        u.branch=b;
        unsigned int hash=(b.address ^ pir)&0x7fff;
        u.btb_set=(b.address>>4)&(BTB_SETS-1);
        u.btb_tag=b.address>>13;
        u.btb_hit=false;u.btb_way=0;
        for(unsigned int j=0;j<4;j++) if(btb[u.btb_set][j].valid &&
          btb[u.btb_set][j].tag==u.btb_tag) {
            u.btb_hit=true;u.btb_way=j;break;
        }
        u.ibtb_set=(hash>>6)&255;
        u.ibtb_tag=(hash&63)|((hash>>14)<<6);
        u.ibtb_hit=false;u.ibtb_way=0;
        for(unsigned int j=0;j<4;j++) if(ibtb[u.ibtb_set][j].valid &&
          ibtb[u.ibtb_set][j].tag==u.ibtb_tag) {
            u.ibtb_hit=true;u.ibtb_way=j;break;
        }
        u.loop_set=(b.address>>4)&63;
        u.loop_tag=b.address>>10;
        u.loop_hit=false;u.loop_way=0;u.used_loop=false;
        bool pred=direction.predict(b)->direction_prediction();
        if(b.br_flags&BR_CONDITIONAL) {
            for(unsigned int j=0;j<2;j++) if(loop[u.loop_set][j].valid &&
              loop[u.loop_set][j].tag==u.loop_tag) {
                u.loop_hit=true;u.loop_way=j;break;
            }
            if(u.loop_hit) {
                loop_entry &e=loop[u.loop_set][u.loop_way];
                if(e.confidence>=3 && e.limit>0) {
                    u.used_loop=true;
                    pred=(e.count==e.limit)?!bool(e.dir):bool(e.dir);
                }
            }
        } else pred=true;
        u.direction_prediction(pred);
        unsigned int target=0;
        if(pred) {
            if((b.br_flags&BR_INDIRECT)&&u.ibtb_hit)
                target=ibtb[u.ibtb_set][u.ibtb_way].target;
            else if(u.btb_hit) target=btb[u.btb_set][u.btb_way].target;
        } else {
            // x86 fall-through length is not exposed by branch_info;
            // use the observed BTB fall-through when available.
            unsigned int fi=b.address&4095;
            if(fall[fi].valid && fall[fi].tag==b.address) target=fall[fi].target;
        }
        u.target_prediction(target);
        return &u;
    }
    void update(branch_update *p,bool taken,unsigned int target) {
        cpm_update *r=static_cast<cpm_update*>(p);
        direction.update(&direction.u,taken,target);
        if(r->branch.br_flags&BR_CONDITIONAL) {
            unsigned int w=r->loop_way;
            if(!r->loop_hit) {
                w=victim(loop[r->loop_set],2);
                loop_entry &e=loop[r->loop_set][w];
                e.valid=1;e.tag=r->loop_tag;e.dir=!taken;
                e.count=0;e.limit=0;e.confidence=0;
            }
            loop_entry &e=loop[r->loop_set][w];
            if(taken==bool(e.dir)) {
                if(e.count<65535) ++e.count;
            } else {
                if(e.count>0 && e.count==e.limit) {
                    if(e.confidence<3) ++e.confidence;
                } else {e.limit=e.count;e.confidence=0;}
                e.count=0;
            }
            touch(loop[r->loop_set],2,w);
        }
        if(!taken && (r->branch.br_flags&BR_CONDITIONAL)) {
            unsigned int fi=r->branch.address&4095;
            fall[fi].valid=true;fall[fi].tag=r->branch.address;fall[fi].target=target;
        }
        if(taken) {
            unsigned int w=r->btb_hit?r->btb_way:victim(btb[r->btb_set],4);
            target_entry &e=btb[r->btb_set][w];
            e.valid=1;e.tag=r->btb_tag;e.target=target;
            touch(btb[r->btb_set],4,w);
            if(r->branch.br_flags&BR_INDIRECT) {
                unsigned int iw=r->ibtb_hit?r->ibtb_way:victim(ibtb[r->ibtb_set],4);
                target_entry &ie=ibtb[r->ibtb_set][iw];
                ie.valid=1;ie.tag=r->ibtb_tag;ie.target=target;
                touch(ibtb[r->ibtb_set],4,iw);
            }
        }
        unsigned int addr=(r->branch.br_flags&BR_INDIRECT)?target:r->branch.address;
        pir=(((pir ^ addr)<<1)|(taken?1u:0u))&0x7fff;
    }
};
