#include "mybundle.h"

void MyHelloTimer::expire(Event*) {
  resched(a_->helloInterval/1000.0);
  a_->sendHello();
}

void BundleTimer::expire(Event*) {
  a_->checkStorage();
  if ((a_->mgmntBundleBuffer->length() >= 1)||(a_->reTxBundleBuffer->length() >= 1)||(a_->bundleBuffer->length() >= 1))
    resched(0.001); // Check buffers every 1 ms
  else
    resched(0.01);  // If empty, check buffers every 10 ms                  
}

int hdr_bundle::offset_;
static class BundleHeaderClass : public PacketHeaderClass {
public:
  BundleHeaderClass() : PacketHeaderClass("PacketHeader/Bundle", 
					  sizeof(hdr_bundle)) {
    bind_offset(&hdr_bundle::offset_);
  }
} class_bundlehdr;


static class BundleClass : public TclClass {
public:
  BundleClass() : TclClass("Agent/Bundle") {}
  TclObject* create(int, const char*const*) {
    return (new BundleAgent());
  }
} class_bundle;


BundleAgent::BundleAgent() : Agent(PT_DTNBUNDLE),  helloTimer_(this), bundleTimer_(this) {
  bind("helloInterval_", &helloInterval);
  bind("retxTimeout_", &retxTimeout);
  bind("deleteForwarded_", &deleteForwarded);
  bind("cqsize_", &cqsize);
  bind("retxqsize_", &retxqsize);
  bind("qsize_", &qsize);
  bind("ifqCongestionDuration_", &ifqCongestionDuration);
  bind("sentBundles_", &sentBundles);
  bind("receivedBundles_", &receivedBundles);
  bind("duplicateReceivedBundles_", &duplicateReceivedBundles);
  bind("duplicateBundles_", &duplicateBundles);
  bind("deletedBundles_", &deletedBundles);
  bind("forwardedBundles_", &forwardedBundles);
  bind("sentReceipts_", &sentReceipts);
  bind("receivedReceipts_", &receivedReceipts);
  bind("duplicateReceivedReceipts_", &duplicateReceivedReceipts);
  bind("duplicateReceipts_", &duplicateReceipts);
  bind("forwardedReceipts_", &forwardedReceipts);
  bind("avBundleDelay_", &avBundleDelay);
  bind("avReceiptDelay_", &avReceiptDelay);
  bind("avBundleHopCount_", &avBundleHopCount);
  bind("avReceiptHopCount_", &avReceiptHopCount);
  bind("avNumNeighbors_", &avNumNeighbors);
  bind("avLinkLifetime_", &avLinkLifetime);
  bind("antiPacket_", &antiPacket);
  bind("routingProtocol_", &routingProtocol);
  bind("initialSpray_", &initialSpray);
  bind("bundleStorageSize_", &bundleStorageSize);
  bind("congestionControl_", &congestionControl);
  bind("bundleStorageThreshold_", &bundleStorageThreshold);
  bind("CVRR_", &CVRR);
  bind("limitRR_", &limitRR);
  bind("dropStrategy_", &dropStrategy);
  bundleDelaysum=0;
  receiptDelaysum=0;
  bundleHopCountsum=0;
  receiptHopCountsum=0;
  helloTimer_.resched((rand()%helloInterval)/1000.0); // To avoid synchronization
  bundleTimer_.resched((rand()%1000)/1000000.0);      // To avoid synchronization
  neighborFreeBytes=NULL;
  countRR=NULL;
  ownDP=NULL;
  neighborNeighborId=NULL;
  neighborDP=NULL;
  neighborLastSeen=NULL;
  neighborFirstSeen=NULL;
  neighborId=NULL;
  neighborBundleTableSize=NULL;
  neighborBundles=NULL;
  neighbors=0;
  bundleNeighborId=NULL;
  bundleNeighborEid=NULL;
  bundleNeighborFragments=NULL;
  bundleNeighborFragmentNumber=NULL;
  bundleNeighborLastSeen=NULL;
  bundleNeighbors=0;

	myBundleStore = new PacketQueue();
  
  bundleStorage=new PacketQueue();
  reTxBundleStorage=new PacketQueue();
  mgmntBundleStorage=new PacketQueue();
  bundleBuffer=new PacketQueue();
  reTxBundleBuffer=new PacketQueue();
  mgmntBundleBuffer=new PacketQueue();
  ifqueue=0;
  last_transmission=0;
  dropsRR=0;
  repsRR=0;
  lastCVRR=0;

	duplicateFrag = 0;
}

int BundleAgent::command(int argc, const char*const* argv) {
  if(argc == 3) {
    if(strcmp(argv[1], "if-queue") == 0) {
      ifqueue=(PriQueue*) TclObject::lookup(argv[2]);
      if(ifqueue == 0)
		return TCL_ERROR;
      return TCL_OK;
    }
  }
  if (argc == 8) {
    if (strcmp(argv[1], "send") == 0) {
      sentBundles++; // Stats
      if ((mgmntBundleStorage->byteLength() + bundleStorage->byteLength() + atoi(argv[3])) > bundleStorageSize) {
 		return (TCL_OK);
      }
      Packet* pkt=allocpkt();
      hdr_cmn* ch=hdr_cmn::access(pkt);
      ch->size()=atoi(argv[3]);
      hdr_bundle* bh=hdr_bundle::access(pkt);
      bh->originating_timestamp=NOW;
      bh->lifetime=atof(argv[4]);
      bh->type=0;
      bh->source=here_.addr_;
      bh->previous=here_.addr_;
      bh->destination=atoi(argv[2]);
      bh->hop_count=0;
      bh->bundle_id=ch->uid(); // Unique bundle ID
      bh->custody_transfer=atoi(argv[5]);
      bh->return_receipt=atoi(argv[6]);
      bh->priority=atoi(argv[7]);
      bh->sent_tos=0;
      for(int n=0; n < 100; n++) {
		bh->sent_to[n]=-1;
		bh->sent_when[n]=0;
      }
      bh->spray=initialSpray;
      bh->nretx=0;
      bh->initial_timestamp=NOW; 
      bundleStorage->enque(pkt);
      return (TCL_OK);
    }
  }
  return (Agent::command(argc, argv));
}

void BundleAgent::sendHello() {
  Packet* pkt=allocpkt();
  hdr_cmn* ch=hdr_cmn::access(pkt);
  //ch->ptype()=PT_MESSAGE; // This is a hack that puts Hello messages first in the IFQ
  ch->size()=20; // Should be a parameter
  hdr_ip* iph=hdr_ip::access(pkt);
  iph->saddr()=here_.addr_;
  iph->sport()=here_.port_;
  iph->daddr()=IP_BROADCAST;
  iph->dport()=0;
  iph->ttl_=1;
  hdr_bundle* bh=hdr_bundle::access(pkt);
  bh->type=3;
  int i=0;
  bh->neighbors=neighbors;
  bh->drops_RR=dropsRR;
  bh->reps_RR=repsRR;
  while (i < neighbors) {
    /* For PROPHET */
    ownDP[i]=ownDP[i]*0.98; // Time unit: Hello interval
    bh->neighbor_table[i]=neighborId[i];
    bh->dp_table[i]=ownDP[i];
    i++;
  }
  while (i < 100) {
    bh->neighbor_table[i]=-1;
    bh->dp_table[i]=0;
    i++;
  }
  if (congestionControl == 2) {
    if ((dropsRR == 0)&&(bundleStorageThreshold < 0.90))
      bundleStorageThreshold = bundleStorageThreshold + 0.01;
    else {
      if ((dropsRR > 0)&&(bundleStorageThreshold > 0.60))
	bundleStorageThreshold = bundleStorageThreshold * 0.8;
      dropsRR=0;
    }
  }
  bh->freebytes=((int)(bundleStorageThreshold*bundleStorageSize - mgmntBundleStorage->byteLength() - bundleStorage->byteLength()));
  int bundles=mgmntBundleStorage->length() + bundleStorage->length();
  if (bundles>500)
    bundles=500;
  bh->eids=bundles;
  int n=0;
  i=0;
  Packet* bpkt=0;
  while ((n < bundles)&&(i < mgmntBundleStorage->length())) {
    bpkt=mgmntBundleStorage->lookup(i);
    hdr_bundle* bbh=hdr_bundle::access(bpkt);
    bh->eid_table[n]=bbh->bundle_id;
    n++;
    i++;
  }
  i=0;
  while ((n < bundles)&&(i < bundleStorage->length())) {
    bpkt=bundleStorage->lookup(i);
    hdr_bundle* bbh=hdr_bundle::access(bpkt);
    bh->eid_table[n]=bbh->bundle_id;
    n++;
    i++;
  }
  while (n < 500) {
    bh->eid_table[n]=-1;
    n++;
  }
  //send(pkt, (Handler*) 0);
  mgmntBundleBuffer->enque(pkt);
}

void BundleAgent::sendCustAck(Packet* pkt) {
  // Send a report to the previous node
  hdr_ip* iph=hdr_ip::access(pkt);
  hdr_bundle* bh=hdr_bundle::access(pkt);
  Packet* pktret=allocpkt();
  hdr_cmn* chret=hdr_cmn::access(pktret);
  chret->size()=10; // Should be a parameter
  hdr_ip* ipret=hdr_ip::access(pktret);
  ipret->saddr()=here_.addr_;
  ipret->sport()=here_.port_;
  ipret->daddr()=iph->saddr();
  ipret->dport()=iph->sport();
  hdr_bundle* bret=hdr_bundle::access(pktret);
  bret->type=1;
  bret->bundle_id=bh->bundle_id;
  mgmntBundleBuffer->enque(pktret);
}

void BundleAgent::sendRetReceipt(Packet* pkt) {
  // Send a report to the source node
  hdr_bundle* bh=hdr_bundle::access(pkt);
  Packet* pktret=allocpkt();
  hdr_cmn* chret=hdr_cmn::access(pktret);
  hdr_bundle* bhret=hdr_bundle::access(pktret);
  chret->size()=10; // Should be a parameter
  bhret->originating_timestamp=NOW;
  bhret->lifetime=retxTimeout - (NOW - bh->originating_timestamp); // min(retxTimeout - bundledelay, lifetime)
  if (bhret->lifetime > bh->lifetime)
    bhret->lifetime=bh->lifetime;
  bhret->type=2;
  bhret->source=here_.addr_;
  bhret->previous=here_.addr_;
  bhret->destination=bh->source;
  bhret->hop_count=0;
  bhret->bundle_id=chret->uid();
  bhret->reverse_eid=bh->bundle_id;
  bhret->custody_transfer=0;
  bhret->return_receipt=0;
  bhret->priority=2;
  bhret->sent_tos=0;
  for(int n=0; n < 100; n++) {
    bhret->sent_to[n]=-1;
    bhret->sent_when[n]=0;
  }
  bhret->spray=initialSpray;
  bhret->nretx=0;
  bhret->initial_timestamp=NOW;
  mgmntBundleStorage->enque(pktret);
  sentReceipts++; // Stats
}

Packet* BundleAgent::copyBundle(Packet* pkt) {
  Packet* newpkt=allocpkt();
  hdr_cmn* ch=hdr_cmn::access(pkt);
  hdr_bundle* bh=hdr_bundle::access(pkt);
  hdr_cmn* newch=hdr_cmn::access(newpkt);
  hdr_bundle* newbh=hdr_bundle::access(newpkt);
  newch->size()=ch->size();
  newbh->originating_timestamp=bh->originating_timestamp;
  newbh->lifetime=bh->lifetime;
  newbh->type=bh->type;
  newbh->source=bh->source;
  newbh->previous=bh->previous;
  newbh->destination=bh->destination;
  newbh->hop_count=bh->hop_count;
  newbh->bundle_id=bh->bundle_id;
  newbh->reverse_eid=bh->reverse_eid;
  newbh->custody_transfer=bh->custody_transfer;
  newbh->return_receipt=bh->return_receipt;
  newbh->priority=bh->priority;
  newbh->sent_tos=bh->sent_tos;
  for(int n=0; n < 100; n++) {
    newbh->sent_to[n]=bh->sent_to[n];
    newbh->sent_when[n]=bh->sent_when[n];
  }
  newbh->spray=bh->spray;
  newbh->nretx=bh->nretx;
  newbh->initial_timestamp=bh->initial_timestamp;
  return newpkt;
}


/*
*first last 啥意思?:first代表要发的起始分片，last
*/
void BundleAgent::sendBundle(Packet* pkt, int next_hop, int first, int last) {
  hdr_cmn* ch=hdr_cmn::access(pkt);
  hdr_bundle* bh=hdr_bundle::access(pkt);
  //fragments: 分成多少片
  int fragments=(ch->size()/1460)+1; // Fragment to IP packets, save 40 bytes for overhead 
  if ((ch->size()%1460) == 0)
    fragments--;
  int retx=0;
  
  if ((first==0)&&(last==0))
    last=fragments;
  else
  	//retx为1表示只是发送Bundle的一部分
    retx=1;
  for(int i=first; i < last; i++) {
    Packet* newpkt=allocpkt();
    hdr_bundle* newbh=hdr_bundle::access(newpkt);
    hdr_ip* newiph=hdr_ip::access(newpkt);
    hdr_cmn* newch=hdr_cmn::access(newpkt);
    if ((i==(fragments-1))&&((ch->size()%1460) > 0))
      newch->size()=(ch->size()%1460)+40;
    else
      newch->size()=1500;
	//为什么要来一个新Bundle??: 每个包都会有bundle头部。
    newbh->nfragments=fragments;
	//i表示第几段。
    newbh->fragment=i;
    newbh->bundle_size=ch->size();
    newbh->originating_timestamp=bh->originating_timestamp;
    newbh->lifetime=bh->lifetime;
    newbh->type=bh->type;
    newbh->source=bh->source;
    newbh->previous=here_.addr_;
    newbh->destination=bh->destination;
    newbh->hop_count=bh->hop_count+1;
    newbh->bundle_id=bh->bundle_id;
    newbh->reverse_eid=bh->reverse_eid;
    newbh->custody_transfer=bh->custody_transfer;
    newbh->return_receipt=bh->return_receipt;
    newbh->priority=bh->priority;
    newbh->sent_tos=0;
    for(int n=0; n < 100; n++) {
      newbh->sent_to[n]=-1;
      newbh->sent_when[n]=0;
    }
    newbh->spray=bh->spray;
    newbh->nretx=bh->nretx;
    newbh->initial_timestamp=bh->initial_timestamp;
    newch->iface()=UNKN_IFACE.value();
    newch->direction()=hdr_cmn::NONE;
    newiph->saddr()=here_.addr_;
    newiph->sport()=here_.port_;
    newiph->dport()=here_.port_;
    newiph->daddr()=next_hop;
    int buffered=0;
    if (retx==1) {
      int n=0;
      Packet* bufferpkt=0;
	  //在三个queue中找该bundle
      while ((n < ifqueue->length())&&(buffered==0)) { //ifqueue:priority queue ,Interface Queue
		bufferpkt=ifqueue->q_->lookup(n);
		if (HDR_CMN(bufferpkt)->ptype()==PT_DTNBUNDLE) {
		  hdr_bundle* bufferbh=hdr_bundle::access(bufferpkt);
		  hdr_ip* bufferiph=hdr_ip::access(bufferpkt);
		  if ((bufferbh->bundle_id == newbh->bundle_id)&&(bufferiph->daddr() == newiph->daddr())&&(bufferbh->fragment == newbh->fragment))
		    buffered=1;
		  }
		  n++;
      } 
      n=0;
      while ((n < reTxBundleBuffer->length())&&(buffered==0)) {
        bufferpkt=reTxBundleBuffer->lookup(n);
		hdr_bundle* bufferbh=hdr_bundle::access(bufferpkt);
		hdr_ip* bufferiph=hdr_ip::access(bufferpkt);
		if ((bufferbh->bundle_id == newbh->bundle_id)&&(bufferiph->daddr() == newiph->daddr())&&(bufferbh->fragment == newbh->fragment))
	          buffered=1;
		n++;
      }
      n=0;
      while ((n < bundleBuffer->length())&&(buffered==0)) {
        bufferpkt=bundleBuffer->lookup(n);
		hdr_bundle* bufferbh=hdr_bundle::access(bufferpkt);
		hdr_ip* bufferiph=hdr_ip::access(bufferpkt);
		if ((bufferbh->bundle_id == newbh->bundle_id)&&(bufferiph->daddr() == newiph->daddr())&&(bufferbh->fragment == newbh->fragment))
	          buffered=1;
		n++;
      }
    }
    if (buffered==1) 
      Packet::free(newpkt);
    else
      if (retx==1)
		reTxBundleBuffer->enque(newpkt);
      else
		if (newbh->type==2)
	  		mgmntBundleBuffer->enque(newpkt);
		else
	  		bundleBuffer->enque(newpkt);
  }
}

void BundleAgent::dropOldest() {
  int n=0;
  Packet* pkt=0;
  double max_lifetime=0;
  int max_n=0;
  while (n < bundleStorage->length()) {
    pkt=bundleStorage->lookup(n);
    hdr_bundle* bh=hdr_bundle::access(pkt);
    if (NOW - bh->originating_timestamp > max_lifetime) {
      max_lifetime=(NOW - bh->originating_timestamp); 
      max_n=n;
    }
    n++;
  }
  pkt=bundleStorage->lookup(max_n);
  bundleStorage->remove(pkt);
  Packet::free(pkt);
}

void BundleAgent::dropMostSpread() {
  int n=0;
  Packet* pkt=0;
  int max_sent_tos=0;
  int max_n=0;
  while (n < bundleStorage->length()) {
    pkt=bundleStorage->lookup(n);
    hdr_bundle* bh=hdr_bundle::access(pkt);
    if (bh->sent_tos > max_sent_tos) {
      max_sent_tos=bh->sent_tos; 
      max_n=n;
    }
    n++;
  }
  pkt=bundleStorage->lookup(max_n);
  bundleStorage->remove(pkt);
  Packet::free(pkt);
}

void BundleAgent::dropLeastSpread() {
  int n=0;
  Packet* pkt=0;
  int min_sent_tos=10000;
  int min_n=0;
  while (n < bundleStorage->length()) {
    pkt=bundleStorage->lookup(n);
    hdr_bundle* bh=hdr_bundle::access(pkt);
    if (bh->sent_tos < min_sent_tos) {
      min_sent_tos=bh->sent_tos; 
      min_n=n;
    }
    n++;
  }
  pkt=bundleStorage->lookup(min_n);
  bundleStorage->remove(pkt);
  Packet::free(pkt);
}

void BundleAgent::dropRandom() {
  int n=Random::integer(bundleStorage->length());
  Packet* pkt=bundleStorage->lookup(n);
  bundleStorage->remove(pkt);
  Packet::free(pkt);
}

void BundleAgent::removeExpiredBundles(PacketQueue* storage) {
  int n=0;
  Packet* pkt=0;
  while (n < storage->length()) {
    pkt=storage->lookup(n);
    hdr_bundle* bh=hdr_bundle::access(pkt);
    if (NOW - bh->originating_timestamp > bh->lifetime) {
      storage->remove(pkt);
      Packet::free(pkt);
    } else
      n++;
  }
}

int BundleAgent::bundleInStorage(PacketQueue* storage, int eid) {
  int n=0, bundle_found=0;
  while ((n < storage->length())&&(bundle_found == 0)) {
    Packet* pkt=storage->lookup(n);
    hdr_bundle* bh=hdr_bundle::access(pkt);
    if (bh->bundle_id == eid)
      bundle_found=1;
    n++;
  }
  return bundle_found;
}

int BundleAgent::neighborHasBundle(int node, int eid) {
  int j=0, bundle_found=0;
  while ((j < neighborBundleTableSize[node])&&(bundle_found == 0)) {
    if (neighborBundles[node][j] == eid)
      bundle_found=1;
    j++;
  }
  return bundle_found;
}

double BundleAgent::sentTo(Packet* pkt, int next_hop) {
  hdr_bundle* bh=hdr_bundle::access(pkt);
  int j=0;
  while (j < bh->sent_tos) {
    if (bh->sent_to[j] == next_hop)
      return (NOW - bh->sent_when[j]);
    j++;
  }
  return 5000.0;
}

void BundleAgent::updateSentTo(Packet* pkt, int next_hop) {
  hdr_bundle* bh=hdr_bundle::access(pkt);
  int j=0, found=0;
  while ((j < bh->sent_tos)&&(found == 0)) {
    if (bh->sent_to[j] == next_hop)
      found = 1;
    else
      j++;
  }
  if (j < 100) {
    if (found == 0)
      ++bh->sent_tos;
    bh->sent_to[j] = next_hop;
    bh->sent_when[j] = NOW;
  } else {
    for(int k=0; k < 99; k++) {
      bh->sent_to[k]=bh->sent_to[k+1];
      bh->sent_when[k]=bh->sent_when[k+1];
    }
    bh->sent_to[99]=next_hop;
    bh->sent_when[99] = NOW;
  }
}

int BundleAgent::fragmentsBuffered(Packet* pkt, int next_hop) {
  hdr_bundle* bh=hdr_bundle::access(pkt);
  int n=0;
  Packet* bufferpkt=0;
  while (n < ifqueue->length()) {
    bufferpkt=ifqueue->q_->lookup(n);
    if (HDR_CMN(bufferpkt)->ptype()==PT_DTNBUNDLE) {
      hdr_bundle* bufferbh=hdr_bundle::access(bufferpkt);
      hdr_ip* bufferiph=hdr_ip::access(bufferpkt);
      if ((bufferbh->bundle_id == bh->bundle_id)&&(bufferiph->daddr() == next_hop)&&(bh->type == 0))
	return 1;
    }
    n++;
  }
  n=0;
  while (n < reTxBundleBuffer->length()) {
    bufferpkt=reTxBundleBuffer->lookup(n);
    hdr_bundle* bufferbh=hdr_bundle::access(bufferpkt);
    hdr_ip* bufferiph=hdr_ip::access(bufferpkt);
    if ((bufferbh->bundle_id == bh->bundle_id)&&(bufferiph->daddr() == next_hop)&&(bh->type == 0))
      return 1;
    n++;
  }
  n=0;
  while (n < bundleBuffer->length()) {
    bufferpkt=bundleBuffer->lookup(n);
    hdr_bundle* bufferbh=hdr_bundle::access(bufferpkt);
    hdr_ip* bufferiph=hdr_ip::access(bufferpkt);
    if ((bufferbh->bundle_id == bh->bundle_id)&&(bufferiph->daddr() == next_hop)&&(bh->type == 0))
      return 1;
    n++;
  }
  return 0;
}

int BundleAgent::neighborHasBetterDP(int neighbor, int destination) {
  int i=0, found=0;
  double ownDeliveryProbability=0;
  while ((i < neighbors)&&(found == 0)) {
    if (neighborId[i] == destination) {
      found=1;
      ownDeliveryProbability=ownDP[i];
    }
    i++;
  }
  i=0, found=0;
  double neighborDeliveryProbability=0;
  while ((i < 100)&&(found == 0)) {
    if (neighborNeighborId[neighbor][i] == destination) {
      found=1;
      neighborDeliveryProbability=neighborDP[neighbor][i];
    }
    i++;
  }
  if (neighborDeliveryProbability > ownDeliveryProbability) // GRTR    
    return 1;
  else
    return 0;
}

void BundleAgent::checkStorage() {
  cqsize=mgmntBundleStorage->byteLength(); // Stats
  retxqsize=reTxBundleStorage->byteLength(); // Stats
  qsize=bundleStorage->byteLength(); // Stats 
  ifqCongestionDuration=NOW-last_transmission; // Stats 
  
  int m=0, currNumNeighbors=0;
  while (m < neighbors) {
    if (NOW-neighborLastSeen[m] < helloInterval/1000.0)
      currNumNeighbors++;
    else {
      if (neighborFirstSeen[m] > 0) {
		avLinkLifetime=(0.98*avLinkLifetime+0.02*(NOW-neighborFirstSeen[m]));
		neighborFirstSeen[m]=0;
      }
    }
    m++;
  }

  avNumNeighbors=(0.98*avNumNeighbors+0.02*currNumNeighbors);
  
  removeExpiredBundles(mgmntBundleStorage);
  removeExpiredBundles(bundleStorage);
  
  // Wait some time after the last fragment of a bundle has left the buffer
  int n=0;
  Packet* pkt=0;
  while (n < bundleStorage->length()) {
    pkt=bundleStorage->lookup(n);
    hdr_bundle* bh=hdr_bundle::access(pkt);
    int j=0;        ;
    while (j < bh->sent_tos) {
      if (fragmentsBuffered(pkt, bh->sent_to[j]) == 1)
        updateSentTo(pkt, bh->sent_to[j]);
      j++;
    }
    n++;
  }
  
  // Check that there is no congestion in the IFQ
  if (ifqueue->length() > 1)
    return;
  
  last_transmission=NOW;
  
  pkt=0;
  if (mgmntBundleBuffer->length() > 0)
    pkt=mgmntBundleBuffer->deque();
  else
    if (reTxBundleBuffer->length() > 0)
      pkt=reTxBundleBuffer->deque();
    else
      if (bundleBuffer->length() > 0)
	pkt=bundleBuffer->deque();
  
  if (pkt!=0)
  	//调用了父类(Classfier?)的send函数
    send(pkt, 0);

  if (mgmntBundleBuffer->length() >= 1)
    return;
  
  // Request missing fragments
  if ((routingProtocol > 0)&&(mgmntBundleBuffer->length() == 0)) {
    int i=0;
    while (i < bundleNeighbors) {
      if ((bundleNeighborId[i] != -1)&&(NOW-bundleNeighborLastSeen[i] > 2.0)) {
		bundleNeighborLastSeen[i] = NOW;
		int j=0, neighbor_found=0;
		while ((j < neighbors)&&(neighbor_found == 0)) {
		  	if (neighborId[j] == bundleNeighborId[i]) {
		    neighbor_found = 1;
		    if ((NOW-neighborLastSeen[j] > 1.0)||(neighborHasBundle(j, bundleNeighborEid[i]) == 0)) {
		      bundleNeighborId[i] = -1; // Stop ARQ if there is no connection or if the neighbor does not have the bundle anymore
		    }
		    else {
		      int k=0, fragment_found=0;
		      while ((k < bundleNeighborFragmentNumber[i])&&(fragment_found == 0)) {
			if (bundleNeighborFragments[i][k]!=1) {
			  fragment_found=1;		  
			  Packet* nakpkt=allocpkt();
			  hdr_bundle* nakbh=hdr_bundle::access(nakpkt);
			  hdr_ip* nakiph=hdr_ip::access(nakpkt);
			  hdr_cmn* nakch=hdr_cmn::access(nakpkt);
			  nakch->iface()=UNKN_IFACE.value();
			  nakch->direction()=hdr_cmn::NONE;
			  nakch->size()=10; // Should be a parameter
			  nakiph->daddr()=neighborId[j];
			  nakiph->dport()=0;
			  nakiph->saddr()=here_.addr_;
			  nakiph->sport()=here_.port_;
			  nakbh->type=4;
			  nakbh->fragment=k; // First
			  nakbh->bundle_id=bundleNeighborEid[i];
			  while ((k < bundleNeighborFragmentNumber[i])&&(bundleNeighborFragments[i][k]!=1)) {
			    bundleNeighborFragments[i][k]=2;
			    k++;
			  }
			  nakbh->nfragments=k; // Last
			  mgmntBundleBuffer->enque(nakpkt);
			} else
			  k++;
	      }
	    }
	  } else
	    j++;
	}
      }
      i++;
    }
  }
  
  if ((mgmntBundleBuffer->length() >= 1)||(reTxBundleBuffer->length() >= 1)||(bundleBuffer->length() >= 1))
    return;
  
  // Any bundles that should be retransmitted?
  n=0;
  pkt=0;
  while (n < reTxBundleStorage->length()) {
    pkt=reTxBundleStorage->lookup(n);
    hdr_bundle* bh=hdr_bundle::access(pkt);
    if (NOW - bh->originating_timestamp > retxTimeout) {
      bh->originating_timestamp=NOW;
      bh->nretx=bh->nretx+1;
      Packet* newpkt=allocpkt();
      bh->bundle_id=HDR_CMN(newpkt)->uid(); // Need to change the ID, otherwise will be treated as duplicate at the receiver
      Packet::free(newpkt);
      Packet* cpkt = copyBundle(pkt);
      bundleStorage->enque(cpkt);
    }
    n++;
  }

  // Forward return receipts
  if (mgmntBundleBuffer->length() == 0) {
    n=0;
    int length=mgmntBundleStorage->length();
    while (n < length) {
      pkt=mgmntBundleStorage->deque();
      hdr_bundle* bh=hdr_bundle::access(pkt);
      int i=0, forwarded=0;
      while (i < neighbors) {
	if ((neighborId[i] != bh->source)&&(neighborId[i] != bh->previous)&&(NOW-neighborLastSeen[i] < helloInterval/1000.0)&& \
	    (sentTo(pkt, neighborId[i]) > 1.5)&&(neighborHasBundle(i, bh->bundle_id) == 0)) {
	  updateSentTo(pkt, neighborId[i]);
	  sendBundle(pkt, neighborId[i], 0, 0);
	  if (here_.addr_ != bh->source) forwardedReceipts++; // Stats
	  forwarded=1;
	}
	i++;
      }
      if ((deleteForwarded == 1)&&(forwarded == 1))
	Packet::free(pkt);
      else
	mgmntBundleStorage->enque(pkt);
      n++;
    }
  }
  
  // Regular bundles to destination then
  if ((mgmntBundleBuffer->length() == 0)&&(reTxBundleBuffer->length() == 0)&&(bundleBuffer->length() == 0)) {
    n=0;
    int length=bundleStorage->length();
    while (n < length) {
      pkt=bundleStorage->deque();
      hdr_bundle* bh=hdr_bundle::access(pkt);
      int i=0, forwarded=0;
      while ((i < neighbors)&&(forwarded == 0)) {
	if ((neighborId[i] == bh->destination)&&(NOW-neighborLastSeen[i] < helloInterval/1000.0)&&(sentTo(pkt, neighborId[i]) > 10.0)) {
	  updateSentTo(pkt, neighborId[i]);
	  sendBundle(pkt, bh->destination, 0, 0);
	  if (here_.addr_ != bh->source) forwardedBundles++; // Stats
	  forwarded=1;
	  if ((bundleInStorage(reTxBundleStorage, bh->bundle_id) == 0)&&((bh->custody_transfer == 1)||(here_.addr_ == bh->source))) {
	    Packet* cpkt = copyBundle(pkt);
	    reTxBundleStorage->enque(cpkt);
	  }
	}
	i++;
      }
      if ((deleteForwarded == 1)&&(forwarded == 1))
	Packet::free(pkt);
      else
	bundleStorage->enque(pkt);
      // Do not have too many fragments in the buffer
      if (forwarded == 1)
	return;
      n++;
    }
    
    // Re-order: put the least forwarded bundles first
    int m=0;
    pkt=0;
    while (m < bundleStorage->length()) {
      int min_sent_tos=10000;
      int n=0;
      int min_n=0;
      while (n < (bundleStorage->length() - m)) {
	pkt=bundleStorage->lookup(n);
	hdr_bundle* bh=hdr_bundle::access(pkt);
	if (bh->sent_tos < min_sent_tos) {
	  min_sent_tos=bh->sent_tos; 
	  min_n=n;
	}
	n++;
      }
      pkt=bundleStorage->lookup(min_n);
      bundleStorage->remove(pkt);
      bundleStorage->enque(pkt);
      m++;
    }
    
    n=0;
    length=bundleStorage->length();
    while (n < length) {
      pkt=bundleStorage->deque();
      hdr_bundle* bh=hdr_bundle::access(pkt);
      int i=0, forwarded=0;
      while ((i < neighbors)&&(forwarded == 0)) {
	if ((neighborId[i] != bh->destination)&&(neighborId[i] != bh->source)&&(neighborId[i] != bh->previous)&& \
	    (NOW-neighborLastSeen[i] < helloInterval/1000.0)&&(sentTo(pkt, neighborId[i]) > 10.0)&&(neighborHasBundle(i, bh->bundle_id) == 0)&& \
	    ((routingProtocol <= 1)||((routingProtocol == 2)&&(bh->spray > 0))||((routingProtocol == 3)&&(neighborHasBetterDP(i, bh->destination)==1)))) {
	  // Apply "ECN-like" congestion control  
	  if ((congestionControl == 0)||\
	      (((congestionControl == 1)||(congestionControl == 2))&&(neighborFreeBytes[i] > HDR_CMN(pkt)->size()))|| \
	      ((congestionControl == 3)&&(countRR[i] > 0))) {
	    updateSentTo(pkt, neighborId[i]);
	    bh->spray=bh->spray/2;
	    if (countRR[i] >= 1)
	      --countRR[i];
	    neighborFreeBytes[i] = neighborFreeBytes[i] - HDR_CMN(pkt)->size();
	    sendBundle(pkt, neighborId[i], 0, 0);
	    if (here_.addr_ != bh->source) forwardedBundles++; // Stats
	    forwarded=1;
	    if ((bundleInStorage(reTxBundleStorage, bh->bundle_id) == 0)&&((bh->custody_transfer == 1)||(here_.addr_ == bh->source))) {
	      Packet* cpkt = copyBundle(pkt);
	      reTxBundleStorage->enque(cpkt);
	    } 
	  }
	}
	i++;
      }
      if ((deleteForwarded == 1)&&(forwarded == 1))
	Packet::free(pkt);
      else
	bundleStorage->enque(pkt);
      // Do not have too many fragments in the buffer
      if (forwarded == 1)
	return;
      n++;
    }
  }
}

void BundleAgent::recv(Packet* pkt, Handler*) {
	hdr_cmn* ch=hdr_cmn::access(pkt);
	hdr_ip* iph=hdr_ip::access(pkt);
	hdr_bundle* bh=hdr_bundle::access(pkt);
	switch(bh->type){
		case TYPE_ACK:
			recvACK(pkt);
			break;
		case TYPE_DATA:
			recvData(pkt);
			break;
		case TYPE_DISRU:
			recvDisruption(pkt);
			break;
		case TYPE_HELLO:
			recvHello(pkt);
			break;
		case TYPE_NAK:
			recvNAK(pkt);
			break;
		default:
			fprintf(stderr, "Invalid AODV type (%x)\n", bh->type);
			exit(1);
	}
}

/*
*	处理确认，删除缓存对应数据
*/
void BundleAgent::recvACK(Packet * p){

}

/*
*	处理到达数据，分为两大类
*				目的地为本节点/为指定下一跳
*				中间缓存数据
*/
void BundleAgent::recvData(Packet * p){
	hdr_cmn* ch=hdr_cmn::access(p);
	hdr_ip* iph=hdr_ip::access(p);
	hdr_bundle* bh=hdr_bundle::access(p);

	//is fragment?
	if(bh->is_fragment()){
		//收到一个分片，
		//存入分组表
		//是不是重复的分片?
		if(fragMap.count(bh->bundle_id) {
			
			if(!fragMap[bh->bundle_id].count(bh->fragment)){
				(fragMap[bh->bundle_id])[bh->fragment] = p;
			}else{
				duplicateFrag++;
			}
			//检查分片是否已收全
			if(fragMap[bh->bundle_id].size() == bh->nfragments){
				//已收全,调用组装函数
				check_bundle(process_for_reassembly(int bundle_id));
			}
			else{
				//未收全
				
			}
			Packet::free(p);
		}else{
			(fragMap[bh->bundle_id])[bh->fragment] = p;
		}
	}else{
		//完整bundle,没有分片
		//myBundleStore.enque(p);
		check_bundle(p);
	}
}

/*
*	处理断路，(重传)分为本节点断路和其他节点断路
*/
void BundleAgent::recvDisruption(Packet * p){
	
}

/*
*	处理广播Hello数据包，维护邻居列表
*/
void BundleAgent::recvHello(Packet * p){
	
}

/*
*	重传某Bundle或部分
*/
void BundleAgent::recvNAK(Packet * p){
}

/*
*	组装并返回Bundle，最后【清空相应表项】
*/
Packet* BundleAgent::process_for_reassembly(int bundle_id){
	
	
}

/*
*	判断Bundle来源，是目的地为本节点/为指定下一跳 还是
*				中间缓存数据,根据这个来选择入哪个队列
*/
void BundleAgent::check_bundle(Packet * p){
	hdr_bundle* bh=hdr_bundle::access(p);

	if(bh->
}

void Umatrix::increaseCal(Umatrix &matrix, double increase, int orient){
// int inNum1 = (orient+1)%BLOCK;
// int inNum2 = (orient+BLOCK-1)%BLOCK;
// 
// matrix.setUtilValue(inNum1, matrix.getUtilValue(inNum1)+increase*0.5);
// matrix.setUtilValue(inNum2, matrix.getUtilValue(inNum2)+increase*0.5);
}

/*
*	普通效用值计算，按周期计算
*/
void Umatrix::uvalue_block_cal(int orient){
	double dTemple = getUtilValue(orient);
	double dA = exp(0.7*dTemple*dTemple)-0.5;
	dTemple += dA * V;
	//算上衰减
	if (dTemple<1)
	{
		setUtilValue(UP, dTemple);
	}
	increaseCal(dA*V, UP);
	decayCal2();
	decayCal(dA*V, UP);
	synCal();
	
}
/*
*	综合效用值计算
*/
void Umatrix::synCal(){
	for (int orient=0; orient<BLOCK; orient++)
	{
		double synNum0 = getUtilValue(orient);
		double synNum1 = getUtilValue((orient+1)%BLOCK);
		double synNum2 = 0.0;//matrix.getUtilValue((orient+2)%BLOCK);
		double synNum3 = getUtilValue((orient+BLOCK-1)%BLOCK);
		double synNum4 = 0.0;//matrix.getUtilValue((orient+BLOCK-2)%BLOCK);
		double synValue = synNum0 + (synNum1 +synNum3)*0.5 + (synNum2+synNum4)*0.25;
		setSynValue(orient,synValue);
	}
}

void Umatrix::decayCal(double increase, int orient){
	int deNum1 = (orient+4)%BLOCK;
	int deNum2 = (orient+5)%BLOCK;
	int deNum3 = (orient+6)%BLOCK;
	int deNum4 = (orient+7)%BLOCK;
	int deNum5 = (orient+8)%BLOCK;
	int deNum6 = (orient+9)%BLOCK;

	double temp1 = getUtilValue(deNum1)-increase*DERATE_T;
	if(temp1 < 0)
		temp1 = 0;
	double temp2 = getUtilValue(deNum2)-increase*DERATE;
	if(temp2 < 0)
		temp2 = 0;
	double temp3 = getUtilValue(deNum3)-increase*0.5;
	if(temp3 < 0)
		temp3 = 0;
	double temp4 = getUtilValue(deNum1)-increase*0.5;
	if(temp4 < 0)
		temp4 = 0;
	double temp5 = getUtilValue(deNum4)-increase*DERATE;
	if(temp5 < 0)
		temp5 = 0;
	double temp6 = getUtilValue(deNum5)-increase*DERATE_T;
	if(temp6 < 0)
		temp6 = 0;

	setUtilValue(deNum1, temp1); 
	setUtilValue(deNum2, temp2); 
	setUtilValue(deNum3, temp3); 
	setUtilValue(deNum4, temp4); 
	setUtilValue(deNum5, temp5); 
	setUtilValue(deNum6, temp6); 
}

void Umatrix::decayCal2(){
	for (int i=0; i<BLOCK; i++)
	{
		setUtilValue(i, matrix.getUtilValue(i)*CON_DERATE);
	}
}







void BundleAgent::recvbak(Packet* pkt, Handler*) {
  hdr_cmn* ch=hdr_cmn::access(pkt);
  hdr_ip* iph=hdr_ip::access(pkt);
  hdr_bundle* bh=hdr_bundle::access(pkt);
  
  if (((u_int32_t)iph->daddr() == IP_BROADCAST) && (bh->type == 3)) {}
  
  if (bh->type == 4) {
    // Receive nack: retransmit packet
    int n=0, found=0;
    Packet* cpkt=0;
    while ((n < bundleStorage->length())&&(found==0)) {
      cpkt=bundleStorage->lookup(n);
      hdr_bundle* cbh=hdr_bundle::access(cpkt);
      if (cbh->bundle_id == bh->bundle_id) {
		found=1;
		sendBundle(cpkt, iph->saddr(), bh->fragment, bh->nfragments);
      } else
	n++;
    }
    Packet::free(pkt);
    return;
  }
  
  if (bh->type == 0) {
    // Reassembly from IP packets
    int found=0, i=0;
    while ((found == 0)&&(i < bundleNeighbors)) {
      if ((bundleNeighborId[i] == iph->saddr())&&(bundleNeighborEid[i] == bh->bundle_id))
		found=1;
      else
		i++;
    }
    if (found == 0) {
      i=0;
      while ((found == 0)&&(i < bundleNeighbors)) {
		if ((NOW-bundleNeighborLastSeen[i] > 10.0)&&(bh->fragment == 0)) {
		  found=1;
		  bundleNeighborFragments[i]=(int*)calloc(bh->nfragments,sizeof(int));
		  bundleNeighborId[i]=iph->saddr();
		  bundleNeighborEid[i]=bh->bundle_id;
		  for (int j=0; j<bh->nfragments; j++) 
		    bundleNeighborFragments[i][j]=0;
		} else
		  i++;
      }
    }
    if (found == 0) {
      bundleNeighbors++;
      bundleNeighborId=(int*)realloc(bundleNeighborId,bundleNeighbors*sizeof(int));
      bundleNeighborEid=(int*)realloc(bundleNeighborEid,bundleNeighbors*sizeof(int));
      bundleNeighborLastSeen=(double*)realloc(bundleNeighborLastSeen,bundleNeighbors*sizeof(double));
      bundleNeighborFragmentNumber=(int*)realloc(bundleNeighborFragmentNumber,bundleNeighbors*sizeof(int));
      bundleNeighborFragments=(int**)realloc(bundleNeighborFragments,bundleNeighbors*sizeof(int*));
      bundleNeighborFragments[i]=(int*)calloc(bh->nfragments,sizeof(int));
      bundleNeighborId[i]=iph->saddr();
      bundleNeighborEid[i]=bh->bundle_id;
      for (int j=0; j<bh->nfragments; j++) 
		bundleNeighborFragments[i][j]=0;
    }
    bundleNeighborLastSeen[i]=NOW;
    bundleNeighborFragmentNumber[i]=bh->nfragments;
    if (bundleNeighborFragments[i][bh->fragment]==1) {
      // Already received
      Packet::free(pkt);
      return;
    }
    bundleNeighborFragments[i][bh->fragment]=1;
    
    int allFragmentsReceived=1;
    for (int j=0; j<bh->nfragments; j++) {
      if (bundleNeighborFragments[i][j]!=1) {
		allFragmentsReceived=0;
		if (routingProtocol > 0) {
		  if ((bundleNeighborFragments[i][j]==0)&&(j<bh->fragment)) {
		    for (int k=j; k<bh->fragment; k++) 
		      bundleNeighborFragments[i][k]=2;
		    // Send nack to the previous node
		    ch->iface()=UNKN_IFACE.value();
		    ch->direction()=hdr_cmn::NONE;
		    ch->size()=10; // Should be a parameter
		    iph->daddr()=bundleNeighborId[i];
		    iph->dport()=0;
		    iph->saddr()=here_.addr_;
		    iph->sport()=here_.port_;
		    bh->type=4;
		    bh->nfragments=bh->fragment; // End
		    bh->fragment=j; // Start
		    mgmntBundleBuffer->enque(pkt);
		    return;
		  }
		}
      }
    }
    
    if (allFragmentsReceived==0) {
      Packet::free(pkt);
      return;
    }
    
    ch->size()=bh->bundle_size;
    // Receive a bundle
    if (bh->destination == here_.addr_) { // This is the final destination
      // Check if this is the first time we receive this bundle
      int found=0, n=0;
      Packet* bpkt=0;
      while (n < mgmntBundleStorage->length()) {
		bpkt=mgmntBundleStorage->lookup(n);
		hdr_bundle* bbh=hdr_bundle::access(bpkt);
		if (bbh->reverse_eid == bh->bundle_id)
		  found=1;
		n++;
      }
      if (bh->custody_transfer == 1)
		sendCustAck(pkt);
      if (found == 0) {
		if (bh->nretx == 0)
		  Tcl::instance().evalf("%s recv %d %d %3.1f %d %3.6f", name(),bh->source,iph->src_.addr_,(NOW-bh->originating_timestamp)*1000,bh->hop_count,NOW);
		else
		  Tcl::instance().evalf("%s retx_recv %d %d %d %3.1f %d %3.6f", name(),bh->nretx,bh->source,iph->src_.addr_,(NOW-bh->initial_timestamp)*1000,bh->hop_count,NOW);
		repsRR++;
		receivedBundles++; // Stats
		bundleDelaysum=bundleDelaysum+(NOW-bh->originating_timestamp);
		avBundleDelay=bundleDelaysum/receivedBundles;
		bundleHopCountsum=bundleHopCountsum+bh->hop_count;
		avBundleHopCount=bundleHopCountsum/receivedBundles;
		if ((bh->return_receipt == 1)||(antiPacket == 1))
			sendRetReceipt(pkt);
      } else {
		Tcl::instance().evalf("%s duplicate_recv %d %d %3.1f %d %3.6f", name(),bh->source,iph->src_.addr_,(NOW-bh->originating_timestamp)*1000,bh->hop_count,NOW);
		duplicateReceivedBundles++; // Stats
      }
      Packet::free(pkt);
    } else {
      Packet* bpkt=0;
      int n=0;
      // Delete incoming bundle if antipacket found
      if (antiPacket == 1) {
	while (n < mgmntBundleStorage->length()) {
	  bpkt=mgmntBundleStorage->lookup(n);
	  hdr_bundle* bbh=hdr_bundle::access(bpkt);
	  if (bbh->reverse_eid == bh->bundle_id) {
	    Tcl::instance().evalf("%s duplicate_mid_recv %d %d %3.1f %d %3.6f", name(),bh->source,iph->src_.addr_,(NOW-bh->originating_timestamp)*1000,bh->hop_count,NOW);
	    Packet::free(pkt);
	    duplicateBundles++; // Stats
	    return;
	  }
	  n++;
	}
      }
      // Check for possible duplicates and delete this bundle if duplicate already stored
      n=0;
      while (n < reTxBundleStorage->length()) {
		bpkt=reTxBundleStorage->lookup(n);
		hdr_bundle* bbh=hdr_bundle::access(bpkt);
		if (bbh->bundle_id == bh->bundle_id) {
		  Tcl::instance().evalf("%s duplicate_mid_recv %d %d %3.1f %d %3.6f", name(),bh->source,iph->src_.addr_,(NOW-bh->originating_timestamp)*1000,bh->hop_count,NOW);
		  Packet::free(pkt);
		  duplicateBundles++; // Stats
		  return;
		}
		n++;
      }
      n=0;
      while (n < bundleStorage->length()) {
		bpkt=bundleStorage->lookup(n);
		hdr_bundle* bbh=hdr_bundle::access(bpkt);
		if (bbh->bundle_id == bh->bundle_id) {
		  Tcl::instance().evalf("%s duplicate_mid_recv %d %d %3.1f %d %3.6f", name(),bh->source,iph->src_.addr_,(NOW-bh->originating_timestamp)*1000,bh->hop_count,NOW);
		  Packet::free(pkt);
		  duplicateBundles++; // Stats
		  return;
		}
		n++;
      }
      Tcl::instance().evalf("%s mid_recv %d %d %3.1f %d %3.6f", name(),bh->source,iph->src_.addr_,(NOW-bh->originating_timestamp)*1000,bh->hop_count,NOW);
      if (mgmntBundleStorage->byteLength() + bundleStorage->byteLength() + ch->size() > bundleStorageSize) {
	// Maybe we should check if the packet can be forwarded without storing it?
	if (dropStrategy == 0)
	  Packet::free(pkt);
	else {
	  if (dropStrategy == 1)
	    dropOldest();
	  if (dropStrategy == 2)
	    dropMostSpread();
	  if (dropStrategy == 3)
	    dropLeastSpread();
	  if (dropStrategy == 4)
	    dropRandom();
	  if (bh->custody_transfer == 1)
	    sendCustAck(pkt);
	  bundleStorage->enque(pkt);
	}
	deletedBundles++; // Stats
	dropsRR++;
      } else {
	if (bh->custody_transfer == 1)
	  sendCustAck(pkt);
	bundleStorage->enque(pkt);
	repsRR++;
      }
    }
    return;
  }
  
  if (bh->type == 1) {
    // Receive custody ack: delete bundle from storage
    Packet* bpkt=0;
    int n=0, found=0;
    while ((n < reTxBundleStorage->length())&&(found == 0)) {
      bpkt=reTxBundleStorage->lookup(n);
      hdr_bundle* bbh=hdr_bundle::access(bpkt);
      if (bbh->bundle_id == bh->bundle_id) {
	reTxBundleStorage->remove(bpkt);
	Packet::free(bpkt);
	found=1;
      }
      n++;
    }
    Tcl::instance().evalf("%s ack_recv %d %3.6f", name(),iph->src_.addr_ >> Address::instance().NodeShift_[1],NOW);
    Packet::free(pkt);
    return;
  }
  
  if (bh->type == 2) {
    // Receive a return receipt
    if (bh->destination == here_.addr_) { // This is the final destination
      int found=0, n=0;
      Packet* bpkt=0;
      while (n < mgmntBundleStorage->length()) {
	bpkt=mgmntBundleStorage->lookup(n);
	hdr_bundle* bbh=hdr_bundle::access(bpkt);
	if (bbh->bundle_id == bh->bundle_id)
	  found=1;
	n++;
      }      
      if (found == 0) {
	Tcl::instance().evalf("%s ret_recv %d %d %3.1f %d %3.6f", name(),bh->source,iph->src_.addr_,(NOW-bh->originating_timestamp)*1000,bh->hop_count,NOW);
	receivedReceipts++; // Stats
	receiptDelaysum=receiptDelaysum+(NOW-bh->originating_timestamp);
	avReceiptDelay=receiptDelaysum/receivedReceipts;
	receiptHopCountsum=receiptHopCountsum+bh->hop_count;
	avReceiptHopCount=receiptHopCountsum/receivedReceipts;
	Packet* bpkt=0;
	int n=0, found=0;
	while ((n < reTxBundleStorage->length())&&(found == 0)) {
	  bpkt=reTxBundleStorage->lookup(n);
	  hdr_bundle* bbh=hdr_bundle::access(bpkt);
	  if (bbh->bundle_id == bh->reverse_eid) {
	    reTxBundleStorage->remove(bpkt);
	    Packet::free(bpkt);
	    found=1;
	  }
	  n++;
	}
	n=0; found=0;
	while ((n < bundleStorage->length())&&(found == 0)) {
	  bpkt=bundleStorage->lookup(n);
	  hdr_bundle* bbh=hdr_bundle::access(bpkt);
	  if (bbh->bundle_id == bh->reverse_eid) {
	    bundleStorage->remove(bpkt);
	    Packet::free(bpkt);
	    found=1;
	  }
	  n++;
	}
	mgmntBundleStorage->enque(pkt); // Keep the return receipt!
      } else {
	Tcl::instance().evalf("%s ret_duplicate_recv %d %d %3.1f %d %3.6f", name(),bh->source,iph->src_.addr_,(NOW-bh->originating_timestamp)*1000,bh->hop_count,NOW);
	duplicateReceivedReceipts++; // Stats
	Packet::free(pkt);
      }
    } else {
      int n=0;
      Packet* bpkt=0;
      // Check for possible duplicates and delete this return receipt if duplicate already stored
      while (n < mgmntBundleStorage->length()) {
	bpkt=mgmntBundleStorage->lookup(n);
	hdr_bundle* bbh=hdr_bundle::access(bpkt);
	if (bbh->bundle_id == bh->bundle_id) {
	  Tcl::instance().evalf("%s ret_duplicate_mid_recv %d %d %3.1f %d %3.6f", name(),bh->source,iph->src_.addr_,(NOW-bh->originating_timestamp)*1000,bh->hop_count,NOW);
	  Packet::free(pkt);
	  duplicateReceipts++; // Stats
	  return;
	}
	n++;
      }      
      if (antiPacket == 1) {
        // Delete bundle if receipt contained antipacket
        n=0; int packetfound=0;
        while ((n < bundleStorage->length())&&(packetfound == 0)) {
          bpkt=bundleStorage->lookup(n);
          hdr_bundle* bbh=hdr_bundle::access(bpkt);
          if (bbh->bundle_id == bh->reverse_eid) {
            bundleStorage->remove(bpkt);
	    Packet::free(bpkt);
            packetfound=1;
          }
          n++;
        }
      }
      Tcl::instance().evalf("%s ret_mid_recv %d %d %3.1f %d %3.6f", name(),bh->source,iph->src_.addr_,(NOW-bh->originating_timestamp)*1000,bh->hop_count,NOW);
      mgmntBundleStorage->enque(pkt);
    }
    return;
  }
}
