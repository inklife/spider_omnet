//
// Generated file, do not edit! Created by nedtool 5.4 from transactionMsg.msg.
//

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif
#ifndef __TRANSACTIONMSG_M_H
#define __TRANSACTIONMSG_M_H

#include <omnetpp.h>

// nedtool version check
#define MSGC_VERSION 0x0504
#if (MSGC_VERSION!=OMNETPP_VERSION)
#    error Version mismatch! Probably this file was generated by an earlier version of nedtool: 'make clean' should help.
#endif



/**
 * Class generated from <tt>transactionMsg.msg:17</tt> by nedtool.
 * <pre>
 * packet transactionMsg
 * {
 *     double amount;
 *     double timeSent;  //time after start time that job is active
 *     int sender;
 *     int receiver;
 *     int priorityClass;
 *     int transactionId; //is messageID of transactionMsg
 *     bool hasTimeOut;
 *     double timeOut;
 *     int htlcIndex;
 *     int pathIndex;
 *     bool isAttempted;
 *     double largerTxnId;
 *     bool isMarked;
 * }
 * </pre>
 */
class transactionMsg : public ::omnetpp::cPacket
{
  protected:
    double amount;
    double timeSent;
    int sender;
    int receiver;
    int priorityClass;
    int transactionId;
    bool hasTimeOut;
    double timeOut;
    int htlcIndex;
    int pathIndex;
    bool isAttempted;
    double largerTxnId;
    bool isMarked;

  private:
    void copy(const transactionMsg& other);

  protected:
    // protected and unimplemented operator==(), to prevent accidental usage
    bool operator==(const transactionMsg&);

  public:
    transactionMsg(const char *name=nullptr, short kind=0);
    transactionMsg(const transactionMsg& other);
    virtual ~transactionMsg();
    transactionMsg& operator=(const transactionMsg& other);
    virtual transactionMsg *dup() const override {return new transactionMsg(*this);}
    virtual void parsimPack(omnetpp::cCommBuffer *b) const override;
    virtual void parsimUnpack(omnetpp::cCommBuffer *b) override;

    // field getter/setter methods
    virtual double getAmount() const;
    virtual void setAmount(double amount);
    virtual double getTimeSent() const;
    virtual void setTimeSent(double timeSent);
    virtual int getSender() const;
    virtual void setSender(int sender);
    virtual int getReceiver() const;
    virtual void setReceiver(int receiver);
    virtual int getPriorityClass() const;
    virtual void setPriorityClass(int priorityClass);
    virtual int getTransactionId() const;
    virtual void setTransactionId(int transactionId);
    virtual bool getHasTimeOut() const;
    virtual void setHasTimeOut(bool hasTimeOut);
    virtual double getTimeOut() const;
    virtual void setTimeOut(double timeOut);
    virtual int getHtlcIndex() const;
    virtual void setHtlcIndex(int htlcIndex);
    virtual int getPathIndex() const;
    virtual void setPathIndex(int pathIndex);
    virtual bool getIsAttempted() const;
    virtual void setIsAttempted(bool isAttempted);
    virtual double getLargerTxnId() const;
    virtual void setLargerTxnId(double largerTxnId);
    virtual bool getIsMarked() const;
    virtual void setIsMarked(bool isMarked);
};

inline void doParsimPacking(omnetpp::cCommBuffer *b, const transactionMsg& obj) {obj.parsimPack(b);}
inline void doParsimUnpacking(omnetpp::cCommBuffer *b, transactionMsg& obj) {obj.parsimUnpack(b);}


#endif // ifndef __TRANSACTIONMSG_M_H

