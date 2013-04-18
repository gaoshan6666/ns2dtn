#ifndef ns_bundle_h
#define ns_bundle_h

#include "agent.h"
#include "tclcl.h"
#include "packet.h"
#include "address.h"
#include "ip.h"
#include "timer-handler.h"
#include "queue.h"
#include <priqueue.h>
#include "random.h"

#include <map>
#include <cmath>

#define TYPE_DATA   1
#define TYPE_ACK    2
#define TYPE_HELLO  3
#define TYPE_DISRU  4 //断路
#define TYPE_NAK    5 //重传请求

#define PI          3.1416
#define BLOCK 12
#define V 0.02	//速度
#define L_UP 300	//每条路长1010s
#define L_RIGHT 300
#define DERATE 0.25
#define DERATE_T 0.1
#define CON_DERATE 0.985

#define UP 2
#define RIGHT 11
#define DOWN 8
#define LEFT 5


struct hdr_bundle {
    int bundle_id; ///< 用来标识一个bundle ，for fragments
    bool is_fragment; //标识是否是分片
    struct {
        double  utiValue[BLOCK]; //综合效用值
        double  locX;
        double  locY;
        double  speed;
    }node_info;
  double originating_timestamp;
  double initial_timestamp;
  double lifetime;
  int type;
  int source;
  int previous;
  int destination;
  int hop_count;
  int reverse_eid;
  int custody_transfer;
  int return_receipt;
  int priority;
  int neighbors;
  int drops_RR;
  int reps_RR;
  int freebytes;
  int eids;
  int eid_table[500];
  int sent_tos;
  int sent_to[100];
  double sent_when[100];
  int spray;
  int nretx;
  //第几段
  int fragment;
  //总分片数
  int nfragments;
  int bundle_size;
  int neighbor_table[100];
  double dp_table[100];
  // Header access methods
  static int offset_; // required by PacketHeaderManager
  inline static int& offset() { return offset_; }
  inline static hdr_bundle* access(const Packet* p) {
    return (hdr_bundle*) p->access(offset_);
  }
};

class BundleAgent;

class MyHelloTimer : public TimerHandler {
 public:
  MyHelloTimer(BundleAgent* a): TimerHandler() {
    a_=a;
  }
 protected:
  virtual void expire(Event *e);
  BundleAgent* a_;
};

class BundleTimer : public TimerHandler {
 public:
  BundleTimer(BundleAgent* a): TimerHandler() {
    a_=a;
  }
 protected:
  virtual void expire(Event *e);
  BundleAgent* a_;
};

class Umatrix
{
public:
	Umatrix(){
		for(int i=0; i<BLOCK; i++){
			m_utilValue[i] = 0.0;
			m_synValue[i] = 0.0;
		}
	}
	void uvalue_block_cal();
	void synCal();
	void increaseCal(double increase, int orient);
	void decayCal(double increase, int orient);
	void decayCal2();
		
	double getUtilValue(int num){
		return m_utilValue[num];
	}
	void setUtilValue(int num, double dValue){
		m_utilValue[num] = dValue;
	}
	double getSynValue(int num){
		return m_synValue[num];
	}
	void setSynValue(int num, double dValue){
		m_synValue[num] = dValue;
	}

private: 
	double m_utilValue[BLOCK];
	double m_synValue[BLOCK];
};

class BundleAgent : public Agent {
 public:
  BundleAgent();
  virtual int command(int argc, const char*const* argv);
  virtual void recv(Packet*, Handler*);

  void recvData(Packet *p);
  void recvACK(Packet *p);
  void recvHello(Packet *p);
  void recvDisruption(Packet *p);
  void recvNAK(Packet *p);

  void sendHello();
  void sendCustAck(Packet *);
  void sendRetReceipt(Packet *);
  Packet* copyBundle(Packet*);
  void sendBundle(Packet*, int, int, int);
  void checkStorage();
  void removeExpiredBundles(PacketQueue*);
  void dropOldest();
  void dropMostSpread();
  void dropLeastSpread();
  void dropRandom();
  int neighborHasBundle(int, int);
  int bundleInStorage(PacketQueue*, int);
  int neighborHasReceipt(int, int);
  int neighborHasBetterDP(int, int);
  double sentTo(Packet*, int);
  void updateSentTo(Packet*, int);
  int fragmentsBuffered(Packet*, int);
  int deleteForwarded;
  int helloInterval;
  double retxTimeout;
  int cqsize;
  int retxqsize;
  int qsize;
  double ifqCongestionDuration;
  int sentBundles;
  int receivedBundles;
  int duplicateReceivedBundles;
  int duplicateBundles;
  int deletedBundles;
  int forwardedBundles;
  int sentReceipts;
  int receivedReceipts;
  int duplicateReceivedReceipts;
  int duplicateReceipts;
  int forwardedReceipts;
  double bundleDelaysum;
  double avBundleDelay;
  double receiptDelaysum;
  double avReceiptDelay;
  double bundleHopCountsum;
  double avBundleHopCount;
  double receiptHopCountsum;
  double avReceiptHopCount;
  double avNumNeighbors;
  double avLinkLifetime;
  int antiPacket;
  int routingProtocol;
  int initialSpray;
  int bundleStorageSize;
  int congestionControl;
  double bundleStorageThreshold;
  double CVRR;
  double limitRR;
  int dropStrategy;
  PacketQueue* bundleBuffer;
  PacketQueue* reTxBundleBuffer;
  PacketQueue* mgmntBundleBuffer;
    
    int duplicateFrag; //重复收到的分片计数

public:

    Packet* process_for_reassembly(int bundle_id);
	void check_bundle(Packet *p);
    
    //std::map<int, std::map<int,Packet>>* fragMap() const { return fragMap_; }
    void set_duplicateFrag(int l)  { duplicateFrag_ = l; }  

	

private:
    std::map<int, std::map<int,Packet>> fragMap; // 利用bundle id 检索fragment表
    PacketQueue* myBundleStore;
	PacketQueue* midBundleBuffer;
    Umatrix utiMatrix;
    double speed;
    
    
        
  MyHelloTimer helloTimer_;  
  BundleTimer bundleTimer_;  
  double* neighborLastSeen;
  double* neighborFirstSeen;
  int* countRR;
  int* neighborFreeBytes;
  int* neighborId;
  double* ownDP;
  int** neighborNeighborId;
  double** neighborDP;
  int* neighborBundleTableSize;
  int** neighborBundles;
  int neighbors;
  int* bundleNeighborId;
  int* bundleNeighborEid;
  double* bundleNeighborLastSeen; //time of last seen
  int* bundleNeighborFragmentNumber;
  int** bundleNeighborFragments;
  int bundleNeighbors;
  PacketQueue* bundleStorage;
  PacketQueue* reTxBundleStorage;
  PacketQueue* mgmntBundleStorage;
  PriQueue *ifqueue; //ifqueue:priority queue ,Interface Queue
  double last_transmission;
  int dropsRR;
  int repsRR;
  double lastCVRR;
};

#endif // ns_bundle_h
