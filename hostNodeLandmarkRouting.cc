#include "hostNodeLandmarkRouting.h"

// set of landmarks for landmark routing
vector<tuple<int,int>> _landmarksWithConnectivityList = {};
vector<int> _landmarks;

Define_Module(hostNodeLandmarkRouting);

/* generates the probe message for a particular destination and a particur path
 * identified by the list of hops and the path index
 */
routerMsg* hostNodeLandmarkRouting::generateProbeMessage(int destNode, int pathIdx, vector<int> path){
    char msgname[MSGSIZE];
    sprintf(msgname, "tic-%d-to-%d probeMsg [idx %d]", myIndex(), destNode, pathIdx);
    probeMsg *pMsg = new probeMsg(msgname);
    pMsg->setSender(myIndex());
    pMsg->setPathIndex(pathIdx);
    pMsg->setReceiver(destNode);
    pMsg->setIsReversed(false);
    vector<double> pathBalances;
    pMsg->setPathBalances(pathBalances);
    pMsg->setPath(path);

    sprintf(msgname, "tic-%d-to-%d router-probeMsg [idx %d]", myIndex(), destNode, pathIdx);
    routerMsg *rMsg = new routerMsg(msgname);
    rMsg->setRoute(path);

    rMsg->setHopCount(0);
    rMsg->setMessageType(PROBE_MSG);

    rMsg->encapsulate(pMsg);
    return rMsg;
}

/* forwards probe messages for waterfilling alone that appends the current balance
 * to the list of balances
 */
void hostNodeLandmarkRouting::forwardProbeMessage(routerMsg *msg){
    // Increment hop count.
    msg->setHopCount(msg->getHopCount()+1);
    //use hopCount to find next destination
    int nextDest = msg->getRoute()[msg->getHopCount()];

    probeMsg *pMsg = check_and_cast<probeMsg *>(msg->getEncapsulatedPacket());
    if (pMsg->getIsReversed() == false){
        vector<double> *pathBalances = & ( pMsg->getPathBalances());
        (*pathBalances).push_back(nodeToPaymentChannel[nextDest].balanceEWMA);
    }

   if (_loggingEnabled) cout << "forwarding " << msg->getMessageType() << " at " 
       << simTime() << endl;
   send(msg, nodeToPaymentChannel[nextDest].gate);
}


/* overall controller for handling messages that dispatches the right function
 * based on message type in Landmark Routing
 */
void hostNodeLandmarkRouting::handleMessage(cMessage *msg) {
    routerMsg *ttmsg = check_and_cast<routerMsg *>(msg);
    
    //Radhika TODO: figure out what's happening here
    if (simTime() > _simulationLength){
        auto encapMsg = (ttmsg->getEncapsulatedPacket());
        ttmsg->decapsulate();
        delete ttmsg;
        delete encapMsg;
        return;
    } 

    switch(ttmsg->getMessageType()) {
        case PROBE_MSG:
             if (_loggingEnabled) cout<< "[HOST "<< myIndex() 
                 <<": RECEIVED PROBE MSG] "<< ttmsg->getName() << endl;
             handleProbeMessage(ttmsg);
             if (_loggingEnabled) cout<< "[AFTER HANDLING:]  "<< endl;
             break;

        default:
             hostNodeBase::handleMessage(msg);
    }
}

/* main routine for handling a new transaction under landmark routing 
 * In particular, initiate probes at sender and send acknowledgements
 * at the receiver
 */
void hostNodeLandmarkRouting::handleTransactionMessageSpecialized(routerMsg* ttmsg){
    transactionMsg *transMsg = check_and_cast<transactionMsg *>(ttmsg->getEncapsulatedPacket());
    int hopcount = ttmsg->getHopCount();
    vector<tuple<int, double , routerMsg *, Id>> *q;
    int destNode = transMsg->getReceiver();
    int destination = destNode;
    int transactionId = transMsg->getTransactionId();

    // if its at the sender, initiate probes, when they come back,
    // call normal handleTransactionMessage
    if (ttmsg->isSelfMessage()) {
        if (transMsg->getTimeSent() >= _transStatStart && 
            transMsg->getTimeSent() <= _transStatEnd) {
            statNumArrived[destination] += 1; 
            statRateArrived[destination] += 1;
            statAmtArrived[destination] += transMsg->getAmount();
        }

        AckState * s = new AckState();
        s->amtReceived = 0;
        s->amtSent = transMsg->getAmount();
        transToAmtLeftToComplete[transMsg->getTransactionId()] = *s;

        // if destination hasn't been encountered, find paths
        if (nodeToShortestPathsMap.count(destNode) == 0 ){
            vector<vector<int>> kShortestRoutes = getKShortestRoutesLandmarkRouting(transMsg->getSender(), 
                    destNode, _kValue);
            initializePathInfoLandmarkRouting(kShortestRoutes, destNode);
        }
        
        initializeLandmarkRoutingProbes(ttmsg, transMsg->getTransactionId(), destNode);
    }
    else if (ttmsg->getHopCount() ==  ttmsg->getRoute().size() - 1) { 
        // txn is at destination, add to incoming units
        int prevNode = ttmsg->getRoute()[ttmsg->getHopCount()-1];
        map<Id, double> *incomingTransUnits = &(nodeToPaymentChannel[prevNode].incomingTransUnits);
        (*incomingTransUnits)[make_tuple(transMsg->getTransactionId(), transMsg->getHtlcIndex())] = 
            transMsg->getAmount();

        if (_timeoutEnabled) {
            auto iter = find_if(canceledTransactions.begin(),
               canceledTransactions.end(),
               [&transactionId](const tuple<int, simtime_t, int, int, int>& p)
               { return get<0>(p) == transactionId; });

            if (iter!=canceledTransactions.end()){
                canceledTransactions.erase(iter);
            }
        }

        // send ack
        routerMsg* newMsg =  generateAckMessage(ttmsg);
        forwardMessage(newMsg);
        return;
    }
    else {
        cout << "entering this case " << endl;
        assert(false);
    }
}


/* handles the special time out mechanism for waterfilling which is responsible
 * for sending time out messages on all paths that may have seen this txn and 
 * marking the txn as cancelled
 */
void hostNodeLandmarkRouting::handleTimeOutMessage(routerMsg* ttmsg){
    timeOutMsg *toutMsg = check_and_cast<timeOutMsg *>(ttmsg->getEncapsulatedPacket());

    if (ttmsg->isSelfMessage()) { 
        //is at the sender
        int transactionId = toutMsg->getTransactionId();
        int destination = toutMsg->getReceiver();
       
        for (auto p : (nodeToShortestPathsMap[destination])){
            int pathIndex = p.first;
            tuple<int,int> key = make_tuple(transactionId, pathIndex);
            if(_loggingEnabled) {
                cout << "transPathToAckState.count(key): " 
                   << transPathToAckState.count(key) << endl;
                cout << "transactionId: " << transactionId 
                    << "; pathIndex: " << pathIndex << endl;
            }
            
            if (transPathToAckState[key].amtSent != transPathToAckState[key].amtReceived) {
                routerMsg* lrTimeOutMsg = generateTimeOutMessageForPath(
                    nodeToShortestPathsMap[destination][p.first].path, 
                    transactionId, destination);
                // TODO: what if a transaction on two different paths have same next hop?
                int nextNode = (lrTimeOutMsg->getRoute())[lrTimeOutMsg->getHopCount()+1];
                CanceledTrans ct = make_tuple(toutMsg->getTransactionId(), 
                        simTime(), -1, nextNode, destination);
                canceledTransactions.insert(ct);
                forwardMessage(lrTimeOutMsg);
            }
            else {
                transPathToAckState.erase(key);
            }
        }
        delete ttmsg;
    }
    else{
        // at the receiver
        CanceledTrans ct = make_tuple(toutMsg->getTransactionId(),simTime(),
                (ttmsg->getRoute())[ttmsg->getHopCount()-1], -1, toutMsg->getReceiver());
        canceledTransactions.insert(ct);
        ttmsg->decapsulate();
        delete toutMsg;
        delete ttmsg;
    }
}


/* specialized ack handler that does the routine for handling acks
 * across paths. In particular, collects/updates stats for this path alone
 * NOTE: acks are on the reverse path relative to the original sender
 */
void hostNodeLandmarkRouting::handleAckMessageSpecialized(routerMsg* ttmsg) {
    ackMsg *aMsg = check_and_cast<ackMsg *>(ttmsg->getEncapsulatedPacket());
    int receiver = aMsg->getReceiver();
    int pathIndex = aMsg->getPathIndex();
    int transactionId = aMsg->getTransactionId();
    tuple<int,int> key = make_tuple(transactionId, pathIndex);
    
    if (transToAmtLeftToComplete.count(transactionId) == 0){
        cout << "error, transaction " << transactionId 
          <<" htlc index:" << aMsg->getHtlcIndex() 
          << " acknowledged at time " << simTime() 
          << " wasn't written to transToAmtLeftToComplete" << endl;
    }
    else {
        (transToAmtLeftToComplete[transactionId]).amtReceived += aMsg->getAmount();
        statAmtCompleted[receiver] += aMsg->getAmount();

        if (transToAmtLeftToComplete[transactionId].amtReceived == 
                transToAmtLeftToComplete[transactionId].amtSent) {
            nodeToShortestPathsMap[receiver][pathIndex].statRateCompleted += 1;

            if (aMsg->getTimeSent() >= _transStatStart && aMsg->getTimeSent() <= _transStatEnd) {
                statNumCompleted[receiver] += 1; 
                statRateCompleted[receiver] += 1;

                double timeTaken = simTime().dbl() - aMsg->getTimeSent();
                statCompletionTimes[receiver] += timeTaken * 1000;
            }

            transToAmtLeftToComplete.erase(aMsg->getTransactionId());
        }
       
        //increment transaction amount ack on a path. 
        transPathToAckState[key].amtReceived += aMsg->getAmount();
    }
    hostNodeBase::handleAckMessage(ttmsg);
}


/* handler that clears additional state particular to lr 
 * when a cancelled transaction is deemed no longer completeable
 * in particular it clears the state that tracks how much of a
 * transaction is still pending
 * calls the base class's handler after its own handler
 */
void hostNodeLandmarkRouting::handleClearStateMessage(routerMsg *ttmsg) {
    for ( auto it = canceledTransactions.begin(); it!= canceledTransactions.end(); it++){
        int transactionId = get<0>(*it);
        simtime_t msgArrivalTime = get<1>(*it);
        int prevNode = get<2>(*it);
        int nextNode = get<3>(*it);
        int destNode = get<4>(*it);
        
        if (simTime() > (msgArrivalTime + _maxTravelTime)){
            for (auto p : nodeToShortestPathsMap[destNode]) {
                int pathIndex = p.first;
                tuple<int,int> key = make_tuple(transactionId, pathIndex);
                if (transPathToAckState.count(key) != 0) {
                    transPathToAckState.erase(key);
                }
            }
        }
    }
    hostNodeBase::handleClearStateMessage(ttmsg);
}

/* handle Probe Message for Landmark Routing 
 * In essence, is waiting for all the probes, finding those paths 
 * with non-zero bottleneck balance and splitting the transaction
 * amongst them
 */
void hostNodeLandmarkRouting::handleProbeMessage(routerMsg* ttmsg){
    probeMsg *pMsg = check_and_cast<probeMsg *>(ttmsg->getEncapsulatedPacket());
    int transactionId = pMsg->getTransactionId();

    if (simTime() > _simulationLength ){
        ttmsg->decapsulate();
        delete pMsg;
        delete ttmsg;
        return;
    }

    bool isReversed = pMsg->getIsReversed();
    int nextDest = ttmsg->getRoute()[ttmsg->getHopCount()+1];
    
    if (isReversed == true){ 
       //store times into private map, delete message
       int pathIdx = pMsg->getPathIndex();
       int destNode = pMsg->getReceiver();
       double bottleneck = minVectorElemDouble(pMsg->getPathBalances());
       ProbeInfo *probeInfo = &(transactionIdToProbeInfoMap[transactionId]);
       
       probeInfo->probeReturnTimes[pathIdx] = simTime();
       probeInfo->numProbesWaiting -= 1; 
       probeInfo->probeBottlenecks[pathIdx] = bottleneck;

       // once all probes are back
       if (probeInfo->numProbesWaiting == 0){ 
           // find total number of paths
           int numPathsPossible = 0;
           for (auto bottleneck: probeInfo->probeBottlenecks){
               if (bottleneck > 0){
                   numPathsPossible++;
               }
           }
           
           transactionMsg *transMsg = check_and_cast<transactionMsg*>(
                   probeInfo->messageToSend->getEncapsulatedPacket());
           vector<double> amtPerPath(probeInfo->probeBottlenecks.size());

           if (numPathsPossible > 0 && 
                   randomSplit(transMsg->getAmount(), probeInfo->probeBottlenecks, amtPerPath)) {
               statRateAttempted[destNode] += 1;
               statAmtAttempted[destNode] += transMsg->getAmount();
               for (int i = 0; i < amtPerPath.size(); i++) {
                   double amt = amtPerPath[i];
                   if (amt > 0) {
                       tuple<int,int> key = make_tuple(transMsg->getTransactionId(), i); 
                       //update the data structure keeping track of how much sent and received on each path
                       if (transPathToAckState.count(key) == 0){
                           AckState temp = {};
                           temp.amtSent = amt;
                           temp.amtReceived = 0;
                           transPathToAckState[key] = temp;
                        }
                        else {
                            transPathToAckState[key].amtSent =  transPathToAckState[key].amtSent + amt;
                        }

                       // send a new transaction on that path with that amount
                       PathInfo *pathInfo = &(nodeToShortestPathsMap[destNode][i]);
                       routerMsg* lrMsg = generateTransactionMessageForPath(amt, 
                               pathInfo->path, i, transMsg); 
                       handleTransactionMessage(lrMsg, true /*revisit*/);
                   }
               }
           } 
           else {
               statRateFailed[destNode] += 1;
               statAmtFailed[destNode] += transMsg->getAmount();
           }

           probeInfo->messageToSend->decapsulate();
           delete transMsg;
           delete probeInfo->messageToSend;
           transactionIdToProbeInfoMap.erase(transactionId);
           transToAmtLeftToComplete.erase(transactionId);
       }
       
       ttmsg->decapsulate();
       delete pMsg;
       delete ttmsg;
   }
   else { 
       //reverse and send message again from receiver
       pMsg->setIsReversed(true);
       ttmsg->setHopCount(0);
       vector<int> route = ttmsg->getRoute();
       reverse(route.begin(), route.end());
       ttmsg->setRoute(route);
       forwardMessage(ttmsg);
   }
}

/* function to compute a random split across all the paths for landmark routing
 */
bool hostNodeLandmarkRouting::randomSplit(double totalAmt, vector<double> bottlenecks, vector<double> &amtPerPath) {
    vector<double> randomParts;
    for (int i = 0; i < bottlenecks.size() - 1; i++){
        randomParts.push_back(rand() / (RAND_MAX + 1.));
    }
    randomParts.push_back(1);
    sort(randomParts.begin(), randomParts.end());

    amtPerPath[0] = totalAmt * randomParts[0];
    if (amtPerPath[0] > bottlenecks[0])
        return false;

    for (int i = 1; i < bottlenecks.size(); i++) {
        amtPerPath[i] = totalAmt*(randomParts[i] - randomParts[i - 1]);
        if (amtPerPath[i] > bottlenecks[i])
            return false;
    }
    return true;
}

/* initializes the table with the paths to use for Landmark Routing, everything else as 
 * to how many probes are in progress is initialized when probes are sent
 * This function only helps for memoization
 */
void hostNodeLandmarkRouting::initializePathInfoLandmarkRouting(vector<vector<int>> kShortestPaths, 
        int  destNode){ 
    nodeToShortestPathsMap[destNode] = {};
    for (int pathIdx = 0; pathIdx < kShortestPaths.size(); pathIdx++){
        PathInfo temp = {};
        nodeToShortestPathsMap[destNode][pathIdx] = temp;
        nodeToShortestPathsMap[destNode][pathIdx].path = kShortestPaths[pathIdx];
    }
    return;
}


/* initializes the actual balance queries for landmark routing and the state 
 * to keep track of how many of them are currently outstanding and how many of them
 * have returned with what balances
 * the msg passed is the transaction that triggered this set of probes which also
 * corresponds to the txn that needs to be sent out once all probes return
 */
void hostNodeLandmarkRouting::initializeLandmarkRoutingProbes(routerMsg * msg, int transactionId, int destNode){
    ProbeInfo probeInfoTemp =  {};
    probeInfoTemp.messageToSend = msg; //message to send out once all probes return
    probeInfoTemp.probeReturnTimes = {}; //probeReturnTimes[0] is return time of first probe

    for (auto pTemp: nodeToShortestPathsMap[destNode]){
        int pathIndex = pTemp.first;
        PathInfo pInfo = pTemp.second;
        vector<int> path = pInfo.path;
        routerMsg * rMsg = generateProbeMessage(destNode , pathIndex, path);

        //set the transactionId in the generated message
        probeMsg *pMsg = check_and_cast<probeMsg *>(rMsg->getEncapsulatedPacket());
        pMsg->setTransactionId(transactionId);
        forwardProbeMessage(rMsg);

        probeInfoTemp.probeReturnTimes.push_back(-1);
        probeInfoTemp.probeBottlenecks.push_back(-1);
    }

    // set number of probes waiting on to be the number of total paths to 
    // this particular destination (might be less than k, so not safe to use that)
    probeInfoTemp.numProbesWaiting = nodeToShortestPathsMap[destNode].size();
    transactionIdToProbeInfoMap[transactionId] = probeInfoTemp;
    return;
}

/* function that is called at the end of the simulation that
 * deletes any remaining messages in transactionIdToProbeInfoMap
 */
void hostNodeLandmarkRouting::finish() {
    for (auto it = transactionIdToProbeInfoMap.begin(); it != transactionIdToProbeInfoMap.end(); it++) {
        ProbeInfo *probeInfo = &(it->second);
        transactionMsg *transMsg = check_and_cast<transactionMsg*>(
            probeInfo->messageToSend->getEncapsulatedPacket());
        probeInfo->messageToSend->decapsulate();
        delete transMsg;
        delete probeInfo->messageToSend;
    }
    hostNodeBase::finish();
}

