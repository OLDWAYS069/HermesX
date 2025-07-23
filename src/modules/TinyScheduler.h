
/**
 * TinyScheduler.h
 *
 * A lightweight, flexible and highly portable task scheduling library for Arduino.
 * 
 * By @ettoreleandrotognoli with implementation fixes by @sdesalas
 *  
 */

#ifndef __TINY_SCHEDULER__
#define __TINY_SCHEDULER__

#include "Arduino.h"
#include <algorithm>


class Callable {
public:
  virtual void operator()() = 0;
};

typedef unsigned long (*TimeProvider)();
typedef void (*Delay)(unsigned long);


void usDelay(unsigned long us);

class TinyScheduler {
public:

  class Group;

  TinyScheduler(TimeProvider timeProvider, Delay delay);
  static TinyScheduler millis() {
        return TinyScheduler(::millis, (Delay)(+::delay));  // ✅ 強制轉型為 function pointer

  }
  static TinyScheduler micros() {
    return TinyScheduler(::micros, usDelay);
  }
  virtual ~TinyScheduler();
  unsigned long tick();
  void loop();
  bool isEmpty() const;
  unsigned int count() const;
  void debug(Stream& stream) const;

  void clear();
  Group group();

  template<typename Callable>
  TinyScheduler& timeout(unsigned long delta, Callable callable) {
    unsigned long time =  this->timeProvider();
    unsigned long when = time + delta;
    bool overflow = when < time;
    this->addNode((new BaseNode<Callable>(when, callable))->withOverflow(overflow));
    return *this;
  }


  template<typename Callable>
  TinyScheduler& every(unsigned long interval, Callable callable) {
    unsigned long time =  this->timeProvider();
    unsigned long when = time + interval;
    bool overflow = when < time;
    this->addNode((new PeriodicNode<Callable>(when, interval, callable))->withOverflow(overflow));
    return *this;
  }

  template<typename Callable>
  TinyScheduler& every(unsigned long firstInterval, unsigned long interval, Callable callable) {
    unsigned long time =  this->timeProvider();
    unsigned long when = time + firstInterval;
    bool overflow = when < time;
    this->addNode((new PeriodicNode<Callable>(when, interval, callable))->withOverflow(overflow));
    return *this;
  }


  template<typename Callable>
  TinyScheduler& repeat(unsigned int times, unsigned long interval, Callable callable) {
    if(times == 0) return *this;
    unsigned long time =  this->timeProvider();
    unsigned long when = time + interval;
    bool overflow = when < time;
    this->addNode((new RepeatableNode<Callable>(when, times, interval, callable))->withOverflow(overflow));
    return *this;
  }


  template<typename Callable>
  TinyScheduler& repeat(unsigned int times, unsigned long firstInterval,  unsigned long interval, Callable callable) {
    if(times == 0) return *this;
    unsigned long time =  this->timeProvider();
    unsigned long when = time + firstInterval;
    bool overflow = when < time;
    this->addNode((new RepeatableNode<Callable>(when, times, interval, callable))->withOverflow(overflow));
    return *this;
  }

  class Node {
    public:
      Node();
      Node(unsigned long when);
      /**
       * returns true if it is done
       */
      virtual bool run() { return true;};
      virtual ~Node() {};

      bool isAfter(const Node& node) const;
      bool isAfter(unsigned long delta) const;
      bool isAfter(bool overflow, unsigned long delta) const;
      bool isBefore(const Node& node) const;
      bool isBefore(unsigned long delta) const;
      bool isBefore(bool overflow, unsigned long delta) const;
      bool isOverflow() const;
      bool hasNext() const;
      unsigned long leftTime(unsigned long delta) const;

      void setNext(Node* next);
      void remove();
      Node* withGroupId(unsigned long groupId);
      Node* withOverflow(bool overflow);


      virtual void debug(Stream& stream) const;
    private:

      friend TinyScheduler;
      Node* next;
      bool overflow = false;
      unsigned long when;
      unsigned long groupId = 0;
  };


  template<typename Callable>
  class BaseNode: public Node {
    protected:
      Callable callable;
    public:

      bool run() {
        this->callable();
        return true;
      }

      BaseNode(unsigned long when, Callable callable) : Node(when), callable(callable) {

      }
  };

  template<typename Callable>
  class PeriodicNode: public Node {
    protected:
      Callable callable;
      unsigned long interval;
    public:

      bool run() {
        this->callable();
        unsigned long oldWhen = this->when;
        this->when += this->interval;
        this->overflow = this->when < oldWhen;
        return false;
      }

      PeriodicNode(unsigned long when, unsigned long interval, Callable callable) : Node(when), callable(callable), interval(interval) {

      }

      void debug(Stream& stream) const {
          stream.print("RepeatableNode {");
          stream.print(" .groupId=");
          stream.print(this->groupId);
          stream.print(" .when=");
          stream.print(this->when);
          stream.print(" .interval=");
          stream.print(this->interval);
          stream.print(" .overflow=");
          stream.print(this->overflow);
          stream.print(" }");
      }
  };

  template<typename Callable>
  class RepeatableNode: public Node {
    protected:
      Callable callable;
      unsigned int times;
      unsigned long interval;
    public:

      bool run() {
        this->times -= 1;
        this->callable();
        unsigned long oldWhen = this->when;
        this->when += this->interval;
        this->overflow = this->when < oldWhen;
        return this->times == 0;
      }

      RepeatableNode(unsigned long when, unsigned int times, unsigned long interval, Callable callable) : Node(when),  times(times), callable(callable), interval(interval) {

      }

      void debug(Stream& stream) const {
          stream.print("RepeatableNode {");
          stream.print(" .groupId=");
          stream.print(this->groupId);
          stream.print(" .when=");
          stream.print(this->when);
          stream.print(" .times=");
          stream.print(this->times);
          stream.print(" .interval=");
          stream.print(this->interval);
          stream.print(" .overflow=");
          stream.print(this->overflow);
          stream.print(" }");
      }
  };


  class Group {
  public:
    void clear();

    template<typename Callable>
    Group& timeout(unsigned long delta, Callable callable) {
      unsigned long time =  this->scheduler.timeProvider();
      unsigned long when = time + delta;
      bool overflow = when < time;
      this->scheduler.addNode((new BaseNode<Callable>(when, callable))->withGroupId(this->id)->withOverflow(overflow));
      return *this;
    }


    template<typename Callable>
    Group& every(unsigned long interval, Callable callable) {
      unsigned long time =  this->scheduler.timeProvider();
      unsigned long when = time + interval;
      bool overflow = when < time;
      this->scheduler.addNode((new PeriodicNode<Callable>(when, interval, callable))->withGroupId(this->id)->withOverflow(overflow));
      return *this;
    }

    template<typename Callable>
    Group& every(unsigned long firstInterval, unsigned long interval, Callable callable) {
      unsigned long time =  this->scheduler.timeProvider();
      unsigned long when = time + firstInterval;
      bool overflow = when < time;
      this->scheduler.addNode((new PeriodicNode<Callable>(when, interval, callable))->withGroupId(this->id)->withOverflow(overflow));
      return *this;
    }


    template<typename Callable>
    Group& repeat(unsigned int times, unsigned long interval, Callable callable) {
      if(times == 0) return *this;
      unsigned long time =  this->scheduler.timeProvider();
      unsigned long when = time + interval;
      bool overflow = when < time;
      this->scheduler.addNode((new RepeatableNode<Callable>(when, times, interval, callable))->withGroupId(this->id)->withOverflow(overflow));
      return *this;
    }


    template<typename Callable>
    Group& repeat(unsigned int times, unsigned long firstInterval,  unsigned long interval, Callable callable) {
      if(times == 0) return *this;
      unsigned long time =  this->scheduler.timeProvider();
      unsigned long when = time + firstInterval;
      bool overflow = when < time;
      this->scheduler.addNode((new RepeatableNode<Callable>(when, times, interval, callable))->withGroupId(this->id)->withOverflow(overflow));
      return *this;
    }

  private:
    friend TinyScheduler;
    Group(TinyScheduler& scheduler, unsigned long id);
    unsigned long id;
    TinyScheduler& scheduler;
  };

  Node* addNode(Node* newNode);

private:
  friend Group;
  Node head;
  TimeProvider timeProvider;
  Delay delay;
  unsigned long lastTick = 0;
  unsigned long nextGroupId = 1;

  void handleNode(Node* node);
  void handleOverflow();
  void clearGroup(unsigned long groupId);

  unsigned long getNextGroupId() {
  this->nextGroupId = (std::max)(1ul, this->nextGroupId);
 

    return this->nextGroupId++;
  }

};

/*********** IMPLEMENTATION DETAIL  - Originally in TinyScheduler.cc ************/

void usDelay(unsigned long us) {
  delayMicroseconds(us);
}

TinyScheduler::TinyScheduler(TimeProvider timeProvider, Delay delay) : timeProvider(timeProvider), delay(delay) {

}

TinyScheduler::~TinyScheduler() {
  TinyScheduler::Node* node = this->head.next;
  while(node != NULL) {
    auto next = node->next;
    delete node;
    node = next;
  }
}

void TinyScheduler::debug(Stream& stream) const {
  stream.println("TinyScheduler::debug");
  stream.println("TinyScheduler Tasks:");
  TinyScheduler::Node* node = this->head.next;
  while (node != NULL) {
    stream.print("\t");
    node->debug(stream);
    stream.println();
    node = node->next;
  }
  stream.println("-----");
}

bool TinyScheduler::isEmpty() const {
  return !this->head.hasNext();
}

void TinyScheduler::handleOverflow() {
  Node* node = this->head.next;
  this->head.next = NULL;
  while(node != NULL) {
    Node* next = node->next;
    if(!node->isOverflow()) {
      this->handleNode(node);
    }
    else {
      this->addNode(node->withOverflow(false));
    }
    node=next;
  }
}

void TinyScheduler::handleNode(Node* node){
  bool deleteNode = node->run();
  if (deleteNode) {
    delete node;
  }
  else {
    this->addNode(node);
  }
}

unsigned long TinyScheduler::tick() {
  while (this->head.hasNext()) {
    unsigned long delta = this->timeProvider();
    bool overflow = this->lastTick > delta;
    this->lastTick = delta;
    if(overflow) {
      this->handleOverflow();
      continue;
    }
    Node* node = this->head.next;
    if (node->isAfter(overflow, delta)) {
      unsigned long leftTime = node->leftTime(delta);
      return leftTime;
    }
    this->head.setNext(node->next);
    this->handleNode(node);
  }
  return 0;
}

unsigned int TinyScheduler::count() const {
  if(this->isEmpty()) {
    return 0;
  }
  unsigned int counter = 0;
  TinyScheduler::Node* node = this->head.next;
  while(node != NULL) {
    counter += 1;
    node = node->next;
  }
  return counter;
}


void TinyScheduler::loop() {
  while(!this->isEmpty()) {
    const unsigned long wait = this->tick();
    if(wait != 0) {
      this->delay(wait);
    }
  }
}

void TinyScheduler::clear() {
  Node* node = this->head.next;
  this->head.next = NULL;
  while (node != NULL) {
    Node* next = node->next;
    delete node;
    node = next;
  }
}

// ---- NODE -----

TinyScheduler::Node::Node(): when(0) {
  this->next = NULL;
} 

TinyScheduler::Node::Node(unsigned long when): when(when) {
  this->next = NULL;
} 

bool TinyScheduler::Node::isAfter(const TinyScheduler::Node& other) const {
  if(this->overflow == other.overflow) {
    return this->when > other.when;
  }
  return this->overflow;
} 

bool TinyScheduler::Node::isBefore(const TinyScheduler::Node& other) const {
  if(this->overflow == other.overflow) {
    return this->when < other.when;
  }
  return other.overflow;
} 

bool TinyScheduler::Node::isAfter(unsigned long delta) const {
  return this->when > delta;
} 

bool TinyScheduler::Node::isAfter(bool overflow, unsigned long delta) const {
  if(this->overflow == overflow) {
    return this->when > delta;
  }
  return this->overflow;
} 

bool TinyScheduler::Node::isBefore(unsigned long delta) const {
  return this->when < delta;
} 

bool TinyScheduler::Node::isBefore(bool overflow, unsigned long delta) const {
  if(this->overflow == overflow) {
    return this->when > delta;
  }
  return overflow;
} 


unsigned long TinyScheduler::Node::leftTime(unsigned long delta) const {
  return this->when - delta;
} 

bool TinyScheduler::Node::isOverflow() const {
  return this->overflow;
} 



bool TinyScheduler::Node::hasNext() const {
  return this->next != NULL;
}

void TinyScheduler::Node::setNext(TinyScheduler::Node* next) {
  this->next = next;
} 


TinyScheduler::Node* TinyScheduler::Node::withGroupId(unsigned long groupId) {
  this->groupId = groupId;
  return this;
} 

TinyScheduler::Node* TinyScheduler::Node::withOverflow(bool overflow) {
  this->overflow = overflow;
  return this;
} 

void TinyScheduler::Node::debug(Stream& stream) const {
  stream.print("Node {");
  stream.print(" .when=");
  stream.print(this->when);
  stream.print(" .groupId=");
  stream.print(this->groupId);
  stream.print(" .overflow=");
  stream.print(this->overflow);
  stream.print(" }");
} 

TinyScheduler::Node* TinyScheduler::addNode(TinyScheduler::Node* newNode) {
  TinyScheduler::Node* node = &this->head;
  while(node->hasNext() && node->next->isBefore(*newNode)) {
    node = node->next;
  }
  newNode->setNext(node->next);
  node->setNext(newNode);
  return newNode;
}

// ---- GROUP -----

void TinyScheduler::clearGroup(unsigned long groupId) {
  Node* prev = &this->head;
  Node* node = this->head.next;
  while(node != NULL) {
    bool removeIt = node->groupId == groupId;
    if(removeIt) {
      prev->next = node->next;
      delete node;
    }
    else{
      prev = node;
    }
    node = prev->next;
  }
}

TinyScheduler::Group TinyScheduler::group() {
  return Group(*this, this->getNextGroupId());
}


TinyScheduler::Group::Group(TinyScheduler& scheduler, unsigned long id): scheduler(scheduler), id(id) {

}

void TinyScheduler::Group::clear() {
  this->scheduler.clearGroup(this->id);
}

#endif