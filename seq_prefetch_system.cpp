// Author: Justin Funston
// Date: January 2013
// Email: jfunston@sfu.ca

#include <cassert>
#include <cmath>
#include <cstdlib>
#include "misc.h"
#include "cache.h"
#include "system.h"

using namespace std;

void SeqPrefetchSystem::memAccess(unsigned long long address, char rw, unsigned     int tid, bool is_prefetch)
{
   unsigned long long set, tag;
   unsigned int local;
   bool hit;
   cacheState state;

   if(doAddrTrans) {
      address = virtToPhys(address);
   }

// Optimizations for single cache simulation
#ifdef MULTI_CACHE 
   local = tid_to_domain[tid];
   updatePageList(address, local);
#else
   local = 0;
#endif

   set = (address & SET_MASK) >> SET_SHIFT;
   tag = address & TAG_MASK;
   state = cpus[local]->findTag(set, tag);
   hit = (state != INV);

   if(countCompulsory && !is_prefetch) {
      checkCompulsory(address & LINE_MASK);
   }

   // Handle hits 
   if(rw == 'W' && hit) {  
      cpus[local]->changeState(set, tag, MOD);
#ifdef MULTI_CACHE
      setRemoteStates(set, tag, INV, local);
#endif
   }

   if(hit) {
      cpus[local]->updateLRU(set, tag);
      if(!is_prefetch) {
         stats.hits++;
         prefetcher.prefetchHit(address, tid, this);
      }
      return;
   }

   // Now handle miss cases
   cacheState remote_state;
#ifdef MULTI_CACHE
   remote = checkRemoteStates(set, tag, remote_state, local);
#else
   remote_state = INV;
#endif
   cacheState new_state = INV;

   unsigned long long evicted_tag;
   bool writeback = cpus[local]->checkWriteback(set, evicted_tag);
   if(writeback)
      evictTraffic(set, evicted_tag, local, is_prefetch);

#ifdef MULTI_CACHE 
   bool local_traffic = isLocal(address, local);
#else
   bool local_traffic = true;
#endif

   new_state = processMESI(remote_state, rw, is_prefetch, local_traffic);
   cpus[local]->insertLine(set, tag, new_state);
   if(!is_prefetch) {
      prefetcher.prefetchMiss(address, tid, this);
   }
}

SeqPrefetchSystem::SeqPrefetchSystem(unsigned int num_domains, vector<unsigned int> tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            bool count_compulsory /*=false*/, bool do_addr_trans /*=false*/) : 
            System(num_domains, tid_to_domain, line_size, num_lines, assoc,
               count_compulsory, do_addr_trans)
{
   return;
}

