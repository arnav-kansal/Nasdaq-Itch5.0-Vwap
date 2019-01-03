#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <array>

namespace trex{
using OrderId = uint64_t;
using LocateId = uint16_t;
using price_t = uint32_t;
using qty_t = uint32_t;

//constants for printing each trading hour
static const uint64_t EPSILON1 = 10000000000LL;
//hour in nanosecond
static const uint64_t HOUR = 3600000000000LL; 

uint64_t CURRTIME = 0;
// 3 AM
uint64_t LASTPRINTED = HOUR * 3;
// bool indicating end of market hours.
bool END = false;

// using compiler lib __builtin_ functions for network order->little endian
uint64_t reinterpret8bytes(unsigned char *w){
    return __builtin_bswap64(*(reinterpret_cast<uint64_t*>(&w[0])));
}

uint64_t reinterpret6bytes(unsigned char *w){
    return (__builtin_bswap64(*reinterpret_cast<uint64_t*>(&w[0])) & (((1L << 48) - 1) << 16)) >> 16;
}

uint32_t reinterpret4bytes(unsigned char *w){
    return __builtin_bswap32(*(reinterpret_cast<uint32_t*>(&w[0])));
}

uint16_t reinterpret2bytes(unsigned char *w){
    return __builtin_bswap16(*(reinterpret_cast<uint16_t*>(&w[0])));
}
// Order structure holding price, qty and locate id of security.
// locateId is in the range(1, 1<<16)
struct Order{
    price_t _price;
    qty_t _qty;
    LocateId _id;
    Order(){}
    Order(price_t price, qty_t qty, LocateId id) : _price(price), _qty(qty), _id(id){} 
};

// indexed by LocateId
std::array<std::vector<Order>, (1 << 16)> attributedBuyOrders;
std::array<std::string, (1 << 16) > stockSymbols;

// Initial thought of using vector for allBuyOrders indexed by OrderId
// as the OrderIds are almost sequential (for lookups (spatial locality))
// Analysis: min max of keys in this data structure can be from 
// (1000, 150,000,000) with only about 700,000 elements actually inside.
// Need a hash map with no speed loss with sequential keys. 
// Google dense_hash_map fairs poorly on sequential keys.
// using gcc hash map for now.
std::unordered_map<OrderId, Order> allBuyOrders;


// CURRTIME is tracking time using messages and printing at 
// each hour till market closes.
void systemMsg(unsigned char* msg_start){
    CURRTIME = reinterpret6bytes(msg_start + 5);
    if(msg_start[11] == 'E')
        END = true;
}

// populating locatedId -> stock symbol string mappings
void stockRelatedMsg(unsigned char* msg_start){
    LocateId locateId = reinterpret2bytes(msg_start + 1);
    std::string symbol(reinterpret_cast<char*>(msg_start) + 11, 8);
    
    stockSymbols[locateId] = symbol;
    attributedBuyOrders[locateId] = std::vector<Order>();
    attributedBuyOrders[locateId].reserve(1 << 5);
}

// only tracking 'B' orders
void addOrder(unsigned char* msg_start){
    LocateId locateId = reinterpret2bytes(msg_start + 1);
    CURRTIME = reinterpret6bytes(msg_start + 5);
    OrderId orderId = reinterpret8bytes(msg_start + 11);
    if(msg_start[19] == 'B'){
        allBuyOrders[orderId] = Order(
                reinterpret4bytes(msg_start + 32), // price
                reinterpret4bytes(msg_start + 20), // qty
                locateId);
    }
}

// wait for static_if or implement own for code reduction
// orderExec and orderExecPrice are same
void orderExec(unsigned char* msg_start){  
    LocateId locateId = reinterpret2bytes(msg_start + 1);
    CURRTIME = reinterpret6bytes(msg_start + 5);
    OrderId orderId = reinterpret8bytes(msg_start + 11);
    qty_t qty = reinterpret4bytes(msg_start + 19);
    auto pair = allBuyOrders.find(orderId);
    if(pair == allBuyOrders.end())
        return;
    price_t price = pair->second._price;
     
   if(qty == pair->second._qty){
        allBuyOrders.erase(pair); 
    }else{
        qty_t newQty = pair->second._qty - qty;
        allBuyOrders[orderId] = Order(price, newQty, locateId);
    }
    attributedBuyOrders[locateId].push_back(Order(price, qty, locateId));
}

void orderExecPrice(unsigned char* msg_start){
    LocateId locateId = reinterpret2bytes(msg_start + 1);
    CURRTIME = reinterpret6bytes(msg_start + 5);
    OrderId orderId = reinterpret8bytes(msg_start + 11);
    qty_t qty = reinterpret4bytes(msg_start + 19);
    auto pair = allBuyOrders.find(orderId);
    price_t price = reinterpret4bytes(msg_start + 32);
    if(pair == allBuyOrders.end())
        return ;

    // non-printable to be handled at cross??
    //if(msg_start[31] == 'N')
    //    return;

    if(qty == pair->second._qty){
        allBuyOrders.erase(pair); 
    }else{
        qty_t newQty = pair->second._qty - qty;
        allBuyOrders[orderId] = Order(price, newQty, locateId);
    }
    attributedBuyOrders[locateId].push_back(Order(price, qty, locateId));
}

void orderCancel(unsigned char* msg_start){
    //CURRTIME = reinterpret6bytes(msg_start + 5);
    OrderId orderId = reinterpret8bytes(msg_start + 11);
    qty_t cancelledQty = reinterpret4bytes(msg_start + 19);
    auto pair = allBuyOrders.find(orderId);
    if(pair == allBuyOrders.end())
        return ;
    Order order = pair->second;
    order._qty -= cancelledQty;
    if(order._qty == 0)
        allBuyOrders.erase(pair);
    allBuyOrders[orderId] = order;
}

void orderDelete(unsigned char* msg_start){
    //CURRTIME = reinterpret6bytes(msg_start + 5);
    OrderId orderId = reinterpret8bytes(msg_start + 11);
    allBuyOrders.erase(orderId);
}

void orderReplace(unsigned char* msg_start){
    LocateId locateId = reinterpret2bytes(msg_start + 1);
    //CURRTIME = reinterpret6bytes(msg_start + 5);
    OrderId oldId = reinterpret8bytes(msg_start + 11);
    OrderId newId = reinterpret8bytes(msg_start + 19);
    qty_t newQty = reinterpret4bytes(msg_start + 27);
    price_t newPrice = reinterpret4bytes(msg_start + 31);
    auto pair = allBuyOrders.find(oldId);
    if(pair == allBuyOrders.end())
        return;
    allBuyOrders.erase(pair);
    allBuyOrders[newId] = Order(newPrice, newQty, locateId);
}

//  [TODO]: clarify:
//  "Since Trade Messages do not affect the book, however, they may be ignored by
//  firms just looking to build and track the Nasdaq execution system display."
void nonCrossTrade(unsigned char* msg_start){
    /*LocateId locateId = reinterpret2bytes(msg_start + 1);
    CURRTIME = reinterpret6bytes(msg_start + 5);
    qty_t qty = reinterpret4bytes(msg_start + 20);
    price_t price = reinterpret4bytes(msg_start + 32);
    attributedBuyOrders[locateId].push_back(Order(price, qty, locateId));*/
}
// [TODO]: clarify
void crossTrade(unsigned char* msg_start){
}

// [TODO]: clarify
// "If a firm is only using the ITCH feed to build a book, however, it may ignore
// these messages as they have no impact on the current book."
void brokenTrade(unsigned char* msg_start){

}

std::string formatTime(uint64_t currTime){
    int hour = currTime/ HOUR;
    return std::to_string(hour);
}

};//trex
