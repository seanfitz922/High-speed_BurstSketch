
 #include <bits/stdc++.h>        
 #include "BurstDetector.h"
 #include "CorrectBurstDetector.h"
 #include "param.h"
 #include "CMBurstDetector.h"
 
 using std::cout;
 using std::endl;
 using std::fixed;
 using std::map;
 using std::setprecision;
 using std::uint32_t;
 using std::uint64_t;
 
 /* ------------------------------------------------------------------ *
  * helper to ceil‑div 
  * ------------------------------------------------------------------ */
 static inline int
 CeilDivision(double numerator, double denominator)
 {
     int quotient      = static_cast<int>(numerator / denominator);
     double remainder  = numerator - (quotient * denominator);
     if (remainder > 0.0)
         ++quotient;
     return quotient;
 }

 int main()
 {
     /*******************************
      * SECTION 1: parameters
      *******************************/
     int      memInKB              = 60;     // “working” memory (KB)
     double   lowerCaseL           = 0.3;    // threshold ratio l
     double   r12Ratio             = 3.75;   // stage1 / stage2 ratio
 
     /* REFACTOR ME */
     const int MEM_IN_BYTES = memInKB * 1024;   // will reuse?
 
     int threshhold   = static_cast<int>(lowerCaseL * threshold);
     int logSizeOriginal = static_cast<int>( MEM_IN_BYTES /
                            (12 * r12Ratio + 20) / bucket_size );
     int logSizeCopy     = logSizeOriginal;  // pointless copy, never changes, FIXME
 
     /* calculate */
     double tmpScreenSize  = logSizeCopy * r12Ratio * bucket_size;
     int    screenLayerSz  = static_cast<int>(tmpScreenSize);
 
     int cm_size =
         CeilDivision(static_cast<double>(memInKB * 1024),
                          static_cast<double>((window_num + 2) * 4)); // fancy
 
     /*******************************
      * SECTION 2: detector objects
      *******************************/
     BurstDetector       A(screenLayerSz,
                                  threshhold,
                                  logSizeCopy,
                                  threshold);
     CorrectBurstDetector B(threshold);

     CMBurstDetector CM(cm_size,threshold);
 
     uint64_t flowID           = 0ULL;
     uint32_t count   = 0U;
     uint32_t timeStamp        = 0U;
 
     int      pktCounter       = 0;  // counts up to 40 000, then resets
     int      windowCounter    = 0;  // sliding window index
 
     int      totalBursts      = 0;
     int      correctlyFound   = 0;
     double   absoluteRelErr   = 0.0;
 
     map<uint64_t,int> helperMap;      
 
     size_t   lineNo           = 0;     
 
     /*******************************
      * SECTION 4: main read loop
      *******************************/
     while (fin >> flowID >> count >> timeStamp)
     {
         ++lineNo;                      // GET RID OF ME
 
         // bump packet counter and roll window
         ++pktCounter;
         if (pktCounter > 40000) {      // DEFINE ME
             pktCounter = 0;
             ++windowCounter;           // advance logical window
         }
 
         /* insert flow into both detectors */
         A.insert(flowID, windowCounter);
         B.insert(flowID, windowCounter);
         CM.insert(flowID, windowCounter);
 
         /* when we switched window, pktCounter is now zero again */
         if (pktCounter == 0) {
             // blow away map from previous round, this is riht?
             helperMap.clear();
 
             /* accumulate statistics */
             totalBursts    += static_cast<int>(B.V.size());
             correctlyFound += static_cast<int>(A.log.V.size());
 
             // copy A’s internal map into helperMap 
             for (auto const &kv : A.log.V)
                 helperMap[kv.first] = kv.second;
 
             /* compute ARE */
             for (auto const &kv : B.V) {
                 uint64_t key     = kv.first;
                 int      refVal  = kv.second;
                 int      estVal  = helperMap[key];  // default 0 if missing
                 absoluteRelErr  += static_cast<double>(refVal - estVal) /
                                    static_cast<double>(refVal);
             }
         }
     }
 
     /*******************************
      * SECTION 5: final report
      *******************************/
     // normalise ARE by total counts
     if (totalBursts > 0)
         absoluteRelErr /= static_cast<double>(totalBursts);
 
     cout << "Recall: "
          << fixed << setprecision(5)
          << ( totalBursts == 0 ? 0.0
                                : 100.0 * static_cast<double>(correctlyFound)
                                  / static_cast<double>(totalBursts) )
          << "%\n";
 
     cout << "ARE: "
          << fixed << setprecision(5)
          << absoluteRelErr << "\n";

    // cout << "Total Bursts: "
    //     << totalBursts << "\n";
 
    std::unordered_set<uint64_t> cmFlows;
    for (const auto& b : CM.Record)
        cmFlows.insert(b.flow_id);

    int matchedCM = 0;
    for (const auto& b : B.Record)
        if (cmFlows.count(b.flow_id))
            ++matchedCM;

    double cmRecall =
        B.Record.empty() ? 0.0
                         : 100.0 * static_cast<double>(matchedCM)
                           / static_cast<double>(B.Record.size());

    // cout << "Recall (CMBurstDetector, by flow-ID): "
    //      << fixed << setprecision(5)
    //      << cmRecall << "%\n";

    // cout << "CMBurstDetector bursts detected: "
    //      << CM.Record.size() << "\n";


    return 0;
 }
 