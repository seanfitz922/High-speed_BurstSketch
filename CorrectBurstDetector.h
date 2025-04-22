
 #ifndef CORRECTBURSTDETECTOR_H         
 #define CORRECTBURSTDETECTOR_H
 
 #include <bits/stdc++.h>
 #include <map>                           
 #include "Burst.h"
 #include "param.h"
 
 using std::uint32_t;
 using std::uint64_t;
 using std::vector;
 using std::pair;
 using std::map;
 
 /* ------------------------------------------------------------------ *
  *   FIX MAGIC NUMBERS 
  * ------------------------------------------------------------------ */
 #define CBD_MAX_FLOWS  2000000
 static constexpr std::size_t kMaxFlows = 2'000'000;
 
 /*======================================================================
  *                      CorrectBurstDetector  
  *====================================================================*/
 class CorrectBurstDetector
 {
 public:
	 map<uint64_t,int>  F;                 // flow ➜ index 
	 uint32_t           w            = 0;  // current flow count
	 uint32_t           m            = 0;  // threshold
	 uint32_t*          counter[2]   = {nullptr,nullptr};
	 uint32_t*          timestamp    = nullptr;
	 uint64_t*          id           = nullptr;
	 uint32_t           last_ts      = 0;  // last window processed
	 bool               side         = false; // toggles between 0/1
	 vector<Burst>      Record;            // saved burst intervals
	 vector<pair<uint64_t,uint32_t>> V;    // high‑speed flows
 
	 /* ---------- ctor  -------------- */
	 explicit CorrectBurstDetector(int _m)
	 {
		 initialise(_m);
	 }
 
	 /* helper so we can “re‑use” the detector without reallocating
		 */
	 void initialise(int _m)
	 {
		 F.clear();
		 w        = 0;
		 m        = static_cast<uint32_t>(_m);
		 last_ts  = 0;
		 side     = false;
		 Record.clear();
		 V.clear();
 
		 /* allocate static arrays */
		 counter[0] = new uint32_t[kMaxFlows];
		 counter[1] = new uint32_t[kMaxFlows];
		 id         = new uint64_t[kMaxFlows];
		 timestamp  = new uint32_t[kMaxFlows];
 
		 for (std::size_t i = 0; i < kMaxFlows; ++i)
		 {
			 counter[0][i] = 0;
			 counter[1][i] = 0;
		 }
		 for (std::size_t i = 0; i < kMaxFlows; ++i)
		 {
			 id[i]        = 0;
			 timestamp[i] = UINT32_MAX;   
		 }
	 }
 
	 /* -------------------------------------------------------------- */
	 /* update()                   */
	 /* -------------------------------------------------------------- */
	 void update(uint32_t now)
	 {
		 V.clear();
 
		 /* PHASE 1: examine every active flow (up to w) */
		 for (uint32_t i = 0; i < w; ++i)
		 {
			 /* record high‑speed flows in current side buffer */
			 if (counter[side][i] >= m)
				 V.emplace_back(id[i], counter[side][i]);
 
			 /* age out stale timestamps */
			 if (timestamp[i] != UINT32_MAX &&
				 now - timestamp[i] > window_num)
			 {
				 timestamp[i] = UINT32_MAX;
			 }
 
			 /* check end‑of‑burst */
			 if (counter[side][i] <= counter[side ^ 1][i] / 2 &&
				 timestamp[i] != UINT32_MAX)
			 {
				 Record.emplace_back(timestamp[i], now, id[i]);
				 timestamp[i] = UINT32_MAX;
			 }
 
			 /* invalidate timestamp if flow dips below m */
			 if (counter[side][i] < m)
				 timestamp[i] = UINT32_MAX;
 
			 /* possible new burst start */
			 if (counter[side][i] >= 2 * counter[side ^ 1][i] &&
				 counter[side][i] >= m)
				 timestamp[i] = now;
		 }
 
		 /* PHASE 2: rotate the active side and zero it */
		 side ^= 1;
		 for (uint32_t i = 0; i < w; ++i)
			 counter[side][i] = 0;
	 }
 
	 /* -------------------------------------------------------------- */
	 /* insert() — feed a packet/flow occurrence into the detector      */
	 /* -------------------------------------------------------------- */
	 void insert(uint64_t flow_id, uint32_t ts)
	 {
		 /* step 0: advance intermediate windows the long way around */
		 if (ts > last_ts)
		 {
			 for (uint32_t t = last_ts; t < ts; ++t)
				 update(t);
			 last_ts = ts;
		 }
 
		 /* step 1: get index for this flow, inserting it if new */
		 if (F.find(flow_id) == F.end())
		 {
			 id[w]   = flow_id;   // record flow ID
			 F[flow_id] = static_cast<int>(w);
			 ++w;
		 }
 
		 /* step 2: bump the counter in the current side */
		 uint32_t idx = static_cast<uint32_t>(F[flow_id]);
		 ++counter[side][idx];
	 }
 };
 
 #endif /* CORRECTBURSTDETECTOR_H */
 