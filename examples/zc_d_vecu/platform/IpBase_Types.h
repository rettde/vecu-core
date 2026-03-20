#ifndef IPBASE_TYPES_H
#define IPBASE_TYPES_H

#include "Std_Types.h"

#define IPBASE_ETH_PHYS_ADDR_LEN_BYTE  6u

typedef uint32  IpBase_AddrInType;
typedef uint32  IpBase_IPAddressType;
typedef uint16  IpBase_FamilyType;
typedef uint16  IpBase_PortType;
typedef uint8   IpBase_EthPhysAddrType[IPBASE_ETH_PHYS_ADDR_LEN_BYTE];
typedef uint8   IpBase_CopyDataType;
typedef uint8   IpBase_SockIdxType;
typedef uint8   IpBase_ReturnType;
typedef uint8   IpBase_TcpIpEventType;

typedef struct {
    IpBase_FamilyType   sa_family;
    uint8               sa_data[26];
} IpBase_SockAddrType;

typedef struct {
    IpBase_FamilyType   sin_family;
    IpBase_PortType     sin_port;
    IpBase_AddrInType   sin_addr;
} IpBase_SockAddrInType;

typedef struct {
    IpBase_FamilyType   sin6_family;
    IpBase_PortType     sin6_port;
    uint32              sin6_flowinfo;
    uint8               sin6_addr[16];
    uint32              sin6_scope_id;
} IpBase_SockAddrIn6Type;

typedef struct {
    uint32  addr;
} IpBase_AddrIn6Type;

typedef struct {
    uint8*  payload;
    uint16  totLen;
    uint16  len;
} IpBase_PbufType;

#endif /* IPBASE_TYPES_H */
