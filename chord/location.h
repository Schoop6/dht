#ifndef _LOCATION_H_
#define _LOCATION_H_
/*
 *
 * Copyright (C) 2000 Frans Kaashoek (kaashoek@lcs.mit.edu)
 * Copyright (C) 2001 Frans Kaashoek (kaashoek@lcs.mit.edu) and 
 *                    Frank Dabek (fdabek@lcs.mit.edu).
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */


#include "aclnt_chord.h"

typedef callback<void,chordstat>::ptr cbping_t;
typedef callback<void,chordID,bool,chordstat>::ref cbchallengeID_t;
extern cbchallengeID_t cbchall_null;

class chord;
class vnode;

// the identifier for the ihash class
struct hashID {
  hashID () {}
  hash_t operator() (const chordID &ID) const {
    return ID.getui ();
  }
};

struct location;

struct RPC_delay_args {
  chordID ID;
  rpc_program prog;
  int procno;
  ptr<void> in;
  void *out;
  aclnt_cb cb;
  u_int64_t s;

  tailq_entry<RPC_delay_args> q_link;

  RPC_delay_args (chordID _ID, rpc_program _prog, int _procno,
		 ptr<void> _in, void *_out, aclnt_cb _cb, u_int64_t _s) :
    ID (_ID), prog (_prog), procno (_procno), 
		   in (_in), out (_out), cb (_cb), s (_s) {};
    
};


struct rpc_state {
  aclnt_cb cb;
  chordID ID;
  u_int64_t s;
  int progno;
  long seqno;
  rpccb_chord *b;
  int rexmits;
  
  rpc_state (aclnt_cb c, chordID id, u_int64_t S, long s, int p) 
    : cb (c), ID (id), s (S), progno (p), seqno (s), b (NULL), rexmits (0) {};

};

struct sent_elm {
  tailq_entry<sent_elm> q_link;
  long seqno;
  
  sent_elm (long s) : seqno (s) {};
};

struct location {
  int refcnt;	// locs w. refcnt == 0 are in the cache; refcnt > 0 are fingers
  chordID n;
  net_address addr;
  sockaddr_in saddr;
  ihash_entry<location> fhlink;
  tailq_entry<location> cachelink;
  u_int64_t rpcdelay;
  u_int64_t nrpc;
  u_int64_t maxdelay;
  float a_lat;
  float a_var;
  bool alive; // whether this node responded to its last RPC
  bool challenged; // whether this node has been succesfully challenged
  vec<cbchallengeID_t> outstanding_cbs;

  location (chordID &_n, net_address &_r);
  ~location ();
};

class locationtable : public virtual refcount {
  ptr<chord> chordnode;
#ifdef PNODE
  ptr<vnode> myvnode;
#endif /* PNODE */
  
  ihash<chordID,location,&location::n,&location::fhlink,hashID> locs;
  tailq<location, &location::cachelink> cachedlocs;  // the cached location

  size_t good;
  
  int size_cachedlocs;
  int max_cachedlocs;

  u_int64_t rpcdelay;
  u_int64_t nrpc;
  float a_lat;
  float a_var;
  float avg_lat;
  float bf_lat;
  float bf_var;
  u_int64_t nrpcfailed;
  u_int64_t nsent;
  u_int64_t npending;
  u_int64_t nchallenge;
  vec<float> timers;
  vec<float> lat_little;
  vec<float> lat_big;
  vec<float> cwind_time;
  vec<float> cwind_cwind;
  vec<long> acked_seq;
  vec<float> acked_time;
  
  u_long nnodessum;
  u_long nnodes;
  unsigned nvnodes;
  
  int seqno;
  float cwind;
  float ssthresh;
  int left;
  float cwind_cum;
  int num_cwind_samples;
  int num_qed;

  tailq<RPC_delay_args, &RPC_delay_args::q_link> Q;
  tailq<sent_elm, &sent_elm::q_link> sent_Q;

  timecb_t *idle_timer;

  ptr<axprt_dgram> dgram_xprt;
  //ptr<aclnt> dgram_clnt;

  qhash<long, svccb *> octbl;
  
  locationtable ();

  void start_network ();

  void connect_cb (location *l, callback<void, ptr<axprt_stream> >::ref cb, 
		   int fd);
  void doRPCcb (ref<aclnt> c, rpc_state *C, clnt_stat err);
  void doRPCreg_cb (chordID ID, aclnt_cb realcb, clnt_stat err);

  void dorpc_connect_cb(location *l, ptr<axprt_stream> x);
  void chord_connect(chordID ID, callback<void, ptr<axprt_stream> >::ref cb);
  void decrefcnt (location *l);
  void touch_cachedlocs (location *l);
  void add_cachedlocs (location *l);
  void delete_cachedlocs (void);
  void remove_cachedlocs (location *l);

  void update_latency (chordID ID, u_int64_t lat, bool bf);
  void ratecb ();
  void update_cwind (int acked);
  void rexmit_handler (rpc_state *s);
  void timeout (rpc_state *s);
  void enqueue_rpc (RPC_delay_args *args);
  void rpc_done (long seqno);
  void reset_idle_timer ();
  void idle ();
  void setup_rexmit_timer (chordID ID, long *sec, long *nsec);
  void timeout_cb (rpc_state *C);

  void ping_cb (cbping_t cb, clnt_stat err);
  void challenge_cb (int challenge, chordID x,
		     chord_challengeres *res, clnt_stat err);

  
 public:
  locationtable (ptr<chord> _chordnode, int _max_connections);
  locationtable (const locationtable &src);
  
  bool betterpred1 (chordID current, chordID target, chordID newpred);

  size_t size () { return locs.size (); }
  size_t usablenodes () { return good; }
  
#ifdef PNODE
  void setvnode (ptr<vnode> v) { myvnode = v; }
#endif /* PNODE */  
  void incvnodes () { nvnodes++; };
  void replace_estimate (u_long o, u_long n);
  u_long estimate_nodes () { return nnodes; }
  void insertgood (chordID &n, sfs_hostname s, int p);
  void insert (chordID &_n, sfs_hostname _s, int _p,
	       cbchallengeID_t cb);
  location *getlocation (chordID &x);
  void cacheloc (chordID &x, net_address &r,
		 cbchallengeID_t cb);
  void updateloc (chordID &x, net_address &r,
		  cbchallengeID_t cb);
  void increfcnt (chordID &n);
  void decrefcnt (chordID &n);
  bool lookup_anyloc (chordID &n, chordID *r);
  chordID closestsuccloc (chordID x);
  chordID closestpredloc (chordID x);
  net_address & getaddress (chordID &x);
  chordID query_location_table (chordID x);
  //  void changenode (node *n, chordID &n, net_address &r);
  void checkrefcnt (int i);
  void doRPC (chordID &n, rpc_program progno, 
	      int procno, ptr<void> in, 
	      void *out, aclnt_cb cb);

  void doRPC_udp (chordID &n, rpc_program progno, 
		  int procno, ptr<void> in, 
		  void *out, aclnt_cb cb);

  void doRPC_tcp (chordID &n, rpc_program progno, 
		  int procno, ptr<void> in, 
		  void *out, aclnt_cb cb);


  void doRPC_tcp_connect_cb (RPC_delay_args *args, int fd);

  void doRPC_issue (chordID &ID, 
		    rpc_program prog, int procno, 
		    ptr<void> in, void *out, aclnt_cb cb,
		    ref<aclnt> c);

  void ping (chordID ID, cbping_t cb);
  bool alive (chordID &x);
  void challenge (chordID &x, cbchallengeID_t cb);
  bool challenged (chordID &x);
  void stats ();
};

extern bool nochallenges;

#endif /* _LOCATION_H_ */

