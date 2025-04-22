
 #ifndef BURSTDETECTOR_H  
 #define BURSTDETECTOR_H

 #include <bits/stdc++.h>
 
 #include "BOBHash32.h"
 #include "param.h"
 #include "Burst.h"
 
 using std::uint32_t;
 using std::uint64_t;
 using std::vector;
 using std::pair;
 
 static constexpr uint32_t INFTY_U32 = 0xffffffff;
 
 /*======================================================================
  *                          ScreenLayer  (Stage 1)
  *====================================================================*/
 class ScreenLayer
 {
 public:
	 /* ---------- member data (left public “for simplicity”) ---------- */
	 int        size   = 0;                // table size
	 int        m      = 0;                // per‑flow threshold
	 uint32_t*  counter = nullptr;         // counters
	 uint64_t*  id      = nullptr;         // flow IDs
	 BOBHash32* bobhash[hash_num];         // hash family
 
	 ScreenLayer() { /* empty = unusable object until init() called */ }
 
	 ScreenLayer(int _size, int _m)
	 {
		 init(_size, _m);
	 }
 
	 void init(int _size, int _m)
	 {
		 size = _size;
		 m    = _m;
 
		 counter = new uint32_t[size];
		 id      = new uint64_t[size];
 
		 // zero everything—done twice 
		 std::fill(counter, counter + size, 0);
		 std::fill(id,      id      + size, 0);
 
		 for (int i = 0; i < hash_num; ++i)
			 bobhash[i] = new BOBHash32(i + 103);
	 }
 
	 /* wipe the whole table  */
	 void update()
	 {
		 for (int i = 0; i < size; ++i)   
		 {
			 counter[i] = 0;
			 id[i]      = 0;
		 }
	 }
 
	 /* tiny inline helpers */
	 uint32_t lookup(int slot) const  { return counter[slot]; }
 
	 void clear(int slotToZap)
	 {
		 // basic O(d) removal (d = hash_num)
		 uint64_t victim = id[slotToZap];
		 for (int i = 0; i < hash_num; ++i)
		 {
			 int h = bobhash[i]->run(reinterpret_cast<char*>(&victim), 8) % size;
			 if (id[h] == victim)
			 {
				 id[h]      = 0;
				 counter[h] = 0;
			 }
		 }
	 }
 
	 /* main insertion routine
	  * Returns: slot index to clear (≥0) if some flow exceeded m,
	  *          otherwise ‑1.
	  */
	 int insert(uint64_t flow_id)
	 {
		 int clearCandidate = -1;
 
		 /* naive d‑hash “majority counter” scheme */
		 for (int i = 0; i < hash_num; ++i)
		 {
			 int h = bobhash[i]->run(reinterpret_cast<char*>(&flow_id), 8) % size;
 
			 if (id[h] == flow_id)            // existing entry
			 {
				 ++counter[h];
				 if (counter[h] >= m)
					 clearCandidate = h;
			 }
			 else if (id[h] == 0)             // empty cell ➜ adopt flow
			 {
				 id[h]      = flow_id;
				 ++counter[h];
			 }
			 else                             // collision ➜ decay
			 {
				 if (counter[h] > 0)          // guard against underflow
					 --counter[h];
				 if (counter[h] == 0)
					 id[h] = 0;
			 }
		 }
		 return clearCandidate;
	 }
 };
 
 /*======================================================================
  *                              Log  (Stage 2)
  *====================================================================*/
 class Log
 {
 public:
	 /* remove public ! */
	 BOBHash32* bobhash      = nullptr;
	 bool       activeSide   = false;   // toggles between 0 and 1
	 int        size         = 0;
	 int        m            = 0;
	 int        scrThresh    = 0;
 
	 uint32_t** counter[2]   = { nullptr, nullptr };
	 uint64_t** id           = nullptr;
	 uint32_t** timestamp    = nullptr;
 
	 vector<Burst>                Record;
	 vector<pair<uint64_t,uint32_t>> V;   
 
	 /* ---------- ctors ---------- */
	 Log() = default;
 
	 Log(int _size, int _m, int _scrThresh)
	 {
		 init(_size, _m, _scrThresh);
	 }
 
	 void init(int _size, int _m, int _scrThresh)
	 {
		 size      = _size;
		 m         = _m;
		 scrThresh = _scrThresh;
 
		 /* allocate 3D array */
		 for (int b = 0; b < 2; ++b)
		 {
			 counter[b] = new uint32_t*[size];
			 for (int i = 0; i < size; ++i)
				 counter[b][i] = new uint32_t[bucket_size];
		 }
 
		 id        = new uint64_t*[size];
		 timestamp = new uint32_t*[size];
		 for (int i = 0; i < size; ++i)
		 {
			 id[i]        = new uint64_t[bucket_size];
			 timestamp[i] = new uint32_t[bucket_size];
		 }
 
		 /* this seems wrong? ased fix */
		 for (int i = 0; i < size; ++i)
			 for (int j = 0; j < bucket_size; ++j)
			 {
				 counter[0][i][j] = 0;
				 counter[1][i][j] = 0;
				 id[i][j]         = 0;
				 timestamp[i][j]  = INFTY_U32;   
			 }
 
		 bobhash = new BOBHash32(1005);
	 }
 
	 /* -------------------------------------------------------------- */
	 /* update() called once per time‑tick to age entries and detect
	  * 
	  */
	 void update(uint32_t now)
	 {
		 V.clear();
 
		 /* PHASE 1: eviction / expiry */
		 for (int i = 0; i < size; ++i)
			 for (int j = 0; j < bucket_size; ++j)
			 {
				 bool tooOld =
				   (id[i][j] != 0 &&
					timestamp[i][j] != INFTY_U32 &&
					now - timestamp[i][j] > window_num);
 
				 bool belowThresh =
				   (id[i][j] != 0 &&
					counter[0][i][j] < scrThresh &&
					counter[1][i][j] < scrThresh);
 
				 if (tooOld || belowThresh)
				 {
					 counter[0][i][j] = counter[1][i][j] = 0;
					 id[i][j]         = 0;
					 timestamp[i][j]  = INFTY_U32;
				 }
			 }
 
		 /* PHASE 2: burst detection */
		 for (int i = 0; i < size; ++i)
			 for (int j = 0; j < bucket_size; ++j)
			 {
				 if (id[i][j] == 0) continue;
 
				 uint32_t curCnt = counter[activeSide][i][j];
				 uint32_t prevCnt= counter[activeSide ^ 1][i][j];
 
				 if (curCnt <= prevCnt / 2 && timestamp[i][j] != INFTY_U32)
				 {
					 /* record burst */
					 Record.emplace_back(timestamp[i][j], now, id[i][j]);
					 timestamp[i][j] = INFTY_U32;      // reset timer
				 }
				 else if (curCnt < m)
				 {
					 timestamp[i][j] = INFTY_U32;      // not big enough
				 }
				 else if (curCnt >= 2 * prevCnt && curCnt >= m)
				 {
					 timestamp[i][j] = now;            // potential start
				 }
 
				 if (curCnt >= m)
					 V.emplace_back(id[i][j], curCnt); // high‑speed flow
			 }
 
		 /* PHASE 3: rotate stage buffer */
		 activeSide ^= 1;
		 for (int i = 0; i < size; ++i)
			 std::fill(counter[activeSide][i],
					   counter[activeSide][i] + bucket_size, 0);
	 }
 
	 /* fast lookup  */
	 bool lookup(uint64_t flow_id, uint32_t /*time*/)
	 {
		 int h = bobhash->run(reinterpret_cast<char*>(&flow_id), 8) % size;
 
		 for (int j = 0; j < bucket_size; ++j)
			 if (id[h][j] == flow_id)
			 {
				 ++counter[activeSide][h][j];
				 return true;        // found & updated
			 }
		 return false;               // miss
	 }
 
	 /* insert or replace flow; returns true if a new entry is stored */
	 bool insert(uint64_t flow_id,
				 uint32_t /*time*/,
				 uint32_t flow_count)
	 {
		 int h = bobhash->run(reinterpret_cast<char*>(&flow_id), 8) % size;
 
		 /* find slot `slotIdx` (first empty OR min counter) */
		 uint32_t minVal = INFTY_U32;
		 int       slotIdx = 0;
		 bool      foundEmpty = false;
 
		 for (int j = 0; j < bucket_size; ++j)
		 {
			 if (id[h][j] == 0)            // empty wins immediately
			 {
				 slotIdx = j;
				 foundEmpty = true;
				 break;
			 }
 
			 uint32_t curCnt = counter[activeSide][h][j];
			 if (curCnt < minVal)
			 {
				 minVal  = curCnt;
				 slotIdx = j;
			 }
		 }
 
		 /* insert / evict */
		 if (foundEmpty ||
			 flow_count > counter[activeSide][h][slotIdx])
		 {
			 id[h][slotIdx]         = flow_id;
			 timestamp[h][slotIdx]  = INFTY_U32;
			 counter[activeSide][h][slotIdx]     = flow_count;
			 counter[activeSide ^ 1][h][slotIdx] = 0;
			 return true;
		 }
		 return false;
	 }
 };
 
 /*======================================================================
  *                          BurstDetector (wrapper)
  *====================================================================*/
 class BurstDetector
 {
 public:
	 ScreenLayer screen_layer;
	 Log         log;
	 uint64_t    last_ts = 0;     // last processed window index
 
	 BurstDetector() = default;
 
	 BurstDetector(int slSize, int slThresh,
				   int logSize, int logThresh)
	 {
		 screen_layer = ScreenLayer(slSize,  slThresh);
		 log          = Log(logSize, logThresh, slThresh);
	 }
 
	 void insert(uint64_t flow_id, uint32_t ts)
	 {
		 /* update intermediate windows */
		 if (ts > last_ts)
		 {
			 for (uint32_t w = last_ts; w < ts; ++w)
			 {
				 screen_layer.update();
				 log.update(w);
			 }
			 last_ts = ts;
		 }
 
		 /* Stage 2 short‑circuit if already tracked */
		 if (log.lookup(flow_id, ts))
			 return;
 
		 /* Stage 1 processing */
		 int slot = screen_layer.insert(flow_id);
		 if (slot < 0) return;                // nothing crossed threshold
 
		 uint32_t cnt = screen_layer.lookup(slot);
		 if (log.insert(flow_id, ts, cnt))
			 screen_layer.clear(slot);        // promote & reset
	 }
 };
 
 #endif /* BURSTDETECTOR_H */
 