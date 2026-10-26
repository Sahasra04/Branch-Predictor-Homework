"""Independent handout table replay; not an execution of pm_predictor."""
branches=['b1','b3','b1','b2','b3','b1','b3','b1','b2','b3','b1','b2','b1','b2','b3','b1','b3','b1','b2','b3','b1','b3']
outcomes=[1,1,0,0,1,1,1,0,0,1,0,1,0,0,1,1,1,0,0,1,1,1]
expected=['NT','NT','T','T','NT','T','T','T','T','T','T','NT','NT','NT','T','T','T','NT','NT','T','T','T']
expected_hit=[False,False,False,False,False,True,False,False,False,True,True,False,False,True,False,True,False,False,False,True,True,False]
expected_ghr=[0,1,3,6,12,9,3,7,14,12,9,2,5,10,4,9,3,7,14,12,9,3]
addr={'b1':0x44,'b2':0x6c,'b3':0x90}
# Handout local counter initial pattern repeats 0,1,2,3; address bits [5:2].
local=[i%4 for i in range(16)]
# Global handout initializes four row counters in each way to 0..3 and 3..0,
# and retains victim counter when allocating a new tag.
ways=[[{'valid':False,'tag':0,'ctr':(row if way==0 else 3-row),'age':1-way} for way in range(2)] for row in range(4)]
ghr=0; rows=[]
for i,(b,actual) in enumerate(zip(branches,outcomes)):
    idx=(addr[b]>>2)&15
    local_pred=local[idx]>=2
    x=idx^ghr; row=x>>2; tag=x&3
    hit_way=next((j for j,e in enumerate(ways[row]) if e['valid'] and e['tag']==tag),None)
    hit=hit_way is not None
    pred=ways[row][hit_way]['ctr']>=2 if hit else local_pred
    rows.append((i+1,b,actual,ghr,x,row,tag,hit,'T' if pred else 'NT',expected[i]))
    # Train local and global counters.
    local[idx]=max(0,min(3,local[idx]+(1 if actual else -1)))
    if hit: w=hit_way
    else:
        w=next((j for j,e in enumerate(ways[row]) if not e['valid']),None)
        if w is None:w=max(range(2),key=lambda j:ways[row][j]['age'])
        ways[row][w]['valid']=True;ways[row][w]['tag']=tag
    e=ways[row][w]
    e['ctr']=max(0,min(3,e['ctr']+(1 if actual else -1)))
    for j in range(2):ways[row][j]['age']=0 if j==w else 1
    ghr=((ghr<<1)|actual)&15
pred_ok=sum(r[-2]==r[-1] for r in rows)
hit_ok=sum(r[7]==expected_hit[i] for i,r in enumerate(rows))
ghr_ok=sum(r[3]==expected_ghr[i] for i,r in enumerate(rows))
print('Handout-style standalone replay:',pred_ok,'/22 predictions;',hit_ok,'/22 hits;',ghr_ok,'/22 GHR values')
for r in rows:
 if r[-2]!=r[-1]:print('Mismatch',r)
import csv
with open('appendix_b_replay.csv','w',newline='') as f:
 w=csv.writer(f);w.writerow(['step','branch','outcome','GHR','XOR','row','tag','hit','computed','handout']);w.writerows(rows)
