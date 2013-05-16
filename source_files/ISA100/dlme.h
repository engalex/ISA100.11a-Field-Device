/*
* Copyright (C) 2013 Nivis LLC.
* Email:   opensource@nivis.com
* Website: http://www.nivis.com
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, version 3 of the License.
* 
* Redistribution and use in source and binary forms must retain this
* copyright notice.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file
/// @verbatim
/// Author:       Nivis LLC, Mihaela Goloman
/// Date:         December 2007
/// Description:  This file holds definitions of the data link layer - dlme module
/// Changes:      Created
/// Revisions:    1.0
/// @endverbatim
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _NIVIS_DLME_H_
#define _NIVIS_DLME_H_

/*===========================================================================*/
#include "../typedef.h"
#include "config.h"


/*==========================================================================*/

#define BROADCAST_DEST_ADDRESS        (uint16)0x7FFF

typedef enum {
    SMIB_IDX_DL_CH = 0,
    SMIB_IDX_DL_TS_TEMPL,
    SMIB_IDX_DL_NEIGH,
    SMIB_IDX_DL_NEIGH_DIAG,
    SMIB_IDX_DL_SUPERFRAME,
    SMIB_IDX_DL_GRAPH,
    SMIB_IDX_DL_LINK,
    SMIB_IDX_DL_ROUTE,
    
    SMIB_IDX_NO
}LOCAL_SMIB_IDX;

#define DL_NON_INDEXED_ATTR_NO        31

// defines for defaults and ranges:
//#define DLL_CLOCK_EXPIRE_DEFAULT   (1000 / DLL_CLOCK_ACCURACY)

// according with ISA the expiration takes on consideration the local PHY clock precision which is not correct
// the more real approach is to be maximum accepted move speed clock 

//#define DLL_CLOCK_EXPIRE_DEFAULT   40 // correspond to 25 ppm max speed 
#define DLL_CLOCK_EXPIRE_DEFAULT   2*DLL_CLOCK_STALE_DEFAULT 


#define DLL_MAX_BACKOFF_EXP_DEFAULT   5
#define DLL_MAX_BACKOFF_EXP_LOW       3
#define DLL_MAX_BACKOFF_EXP_HIGH      8

#define DLL_MAX_DSDU_SIZE_DEFAULT     96
#define DLL_MAX_DSDU_SIZE_LOW         76
#define DLL_MAX_DSDU_SIZE_HIGH        96

#define DLL_MAX_LIFETIME_DEFAULT      120  // = 30 s 
#define DLL_MAX_LIFETIME_LOW          8    // = 2 s
#define DLL_MAX_LIFETIME_HIGH         1920 // = 480 s

#define DLL_NACK_BACKOFF_DUR_DEFAULT  60   // = 15 s
#define DLL_NACK_BACKOFF_DUR_LOW      8    // = 2 s
#define DLL_NACK_BACKOFF_DUR_HIGH     1920 // = 480 s

#define DLL_LINK_PRIORITY_XMIT_DEFAULT  8
#define DLL_LINK_PRIORITY_XMIT_HIGH     15

#define DLL_LINK_PRIORITY_RCV_DEFAULT   0
#define DLL_LINK_PRIORITY_RCV_HIGH      15

#define DLL_CLOCK_STALE_DEFAULT         45  // unit 1 second
#define DLL_CLOCK_STALE_LOW             5   // unit 1 second
#define DLL_CLOCK_STALE_HIGH            300 // unit 1 second

#define DLL_RADIO_SILENCE_DEFAULT       600 // unit 1 s
#define DLL_RADIO_SILENCE_LOW           1   // for radio silence profile
#define DLL_RADIO_SILENCE_HIGH          600 // for radio silence profile

#define DLL_RADIO_TRANSMIT_POWER_DEFAULT  10  // units dBm - maximum supported by the VN210 - calibration done for this value
#define DLL_RADIO_TRANSMIT_POWER_LOW      -20
#define DLL_RADIO_TRANSMIT_POWER_HIGH     36  

#define DLL_COUNTRY_CODE_DEFAULT        0x0400
#define DLL_COUNTRY_CODE_HIGH           0x2000

#define DLL_DISCOVERY_ALERT_DEFAULT     60

enum
{
  DL_CONNECTIVITY_ALERT    = 0,
  DL_NEIGH_DISCOVERY_ALERT = 1,
  DL_CUSTOM_DIAG_ALERT = 101
}; // DLMO_ALERT_TYPES


enum
{
  DL_ACT_SCAN_HOST_FRACT  = 1,
  DL_ADV_JOIN_INFO        = 2,
  DL_ADV_SUPERFRAME       = 3,
  DL_SUBNET_ID            = 4,
  DL_SOLIC_TEMPL          = 5,
  DL_ADV_FILTER           = 6,
  DL_SOLIC_FILTER         = 7,
  DL_TAI_TIME             = 8,
  DL_TAI_ADJUST           = 9,
  DL_MAX_BACKOFF_EXP      = 10,
  DL_MAX_DSDU_SIZE        = 11,
  DL_MAX_LIFETIME         = 12,
  DL_NACK_BACKOFF_DUR     = 13,
  DL_LINK_PRIORITY_XMIT   = 14,
  DL_LINK_PRIORITY_RCV    = 15,  
  DL_ENERGY_DESIGN        = 16,
  DL_ENERGY_LEFT          = 17,  
  DL_DEV_CAPABILITY       = 18,  
  DL_IDLE_CHANNELS        = 19,  
  DL_CLOCK_EXPIRE         = 20,  
  DL_CLOCK_STALE          = 21,  
  DL_RADIO_SILENCE        = 22,  
  DL_RADIO_SLEEP          = 23,  
  DL_RADIO_TRANSMIT_POWER = 24,  
  DL_COUNTRY_CODE         = 25,  
  DL_CANDIDATES           = 26,  
  DL_DISCOVERY_ALERT      = 27,  
  DL_SMOOTH_FACTORS       = 28,  
  DL_QUEUE_PRIOTITY       = 29,
  DL_CH                   = 30,  
  DL_CH_META              = 31,  
  DL_TS_TEMPL             = 32,
  DL_TS_TEMPL_META        = 33,    
  DL_NEIGH                = 34,
  DL_NEIGH_DIAG_RESET     = 35,
  DL_NEIGH_META           = 36, 
  DL_SUPERFRAME           = 37,
  DL_SF_IDLE              = 38,
  DL_SUPERFRAME_META      = 39,  
  DL_GRAPH                = 40,
  DL_GRAPH_META           = 41,
  DL_LINK                 = 42,
  DL_LINK_META            = 43,  
  DL_ROUTE                = 44,
  DL_ROUTE_META           = 45,
  DL_NEIGH_DIAG           = 46,
  DL_NEIGH_DIAG_META      = 47,
  DL_CH_DIAG              = 48,
  DL_ALERT_POLICY         = 49,
    
  DL_NO 
}; // DLMO_ATTRIBUTE_IDS


// channel hop pattern
typedef struct
{
  uint16  m_unUID;
  uint8   m_ucLength;   // range 1 to 16 (not 0 to 15!!!)
  uint8   m_aSeq[8];    // channel hopping pattern; 16 elements of 4 bits each (High nibble, Low nibble, ...)
}DLL_SMIB_CHANNEL;  // 1 byte alignment

// receive template
typedef struct
{  
  uint16  m_unEarliestDPDU; // earliest time when DPDU reception can be expected to begin, indicating when to enable
                            // radio's receiver; offset from timeslot's nominalscheduled start time; units 2^(-20) sec
  uint16  m_unTimeoutDPDU;  // latest time when DPDU reception can be expected to begin, indicating when to disable 
                            // receiver if no incoming DPDU is detected; offset from timeslot's nominal scheduled start time; units 2^(-20) sec
  uint16  m_unAckEarliest;  // start of acknowledgement delay time range; reference depends on AckRef; 
                            // if AckRef=1, subtract value from nominal scheduled end time of timeslot to determine AckEarliest; units 2^(-20) sec
  uint16  m_unAckLatest;    // end of acknowledgement delay time range; reference depends on AckRef; 
                            // if AckRef=1, subtract value from nominal scheduled end time of timeslot to determine AckEarliest; units 2^(-20) sec
  
  uint16  m_unEnableReceiverTMR2; // on TMR2 steps
  uint16  m_unTimeoutTMR2;        // on TMR2 steps
  uint16  m_unAckEarliestTMR2;    // on TMR2 steps
  uint16  m_unAckLatestTMR2;      // on TMR2 steps
}DLL_TIMESLOT_RECEIVE; // 2 bytes alignment

// transmit template
typedef struct
{   
  uint16  m_unXmitEarliest; // earliest time to start DPDU transmission; 
                            // offset from timeslot's nominal scheduled start time; units 2^(-20) sec
  uint16  m_unXmitLatest;   // latest time to start DPDU transmission; 
                            // offset from timeslot's nominalscheduled start time; units 2^(-20) sec
  uint16  m_unAckEarliest;  // start of acknowledgement delay time range; enable receiver 
                            // early enough to receive a DPDU beginning at this time; units 2^(-20) sec
  uint16  m_unAckLatest;    // end of acknowledgement delay time range; 
                            // may disable receiver if DPDU reception has not started by this time; units 2^(-20) sec
  
  uint16 m_unXmitEarliestTMR2;  // on TMR2 steps
  uint16 m_unXmitLatestTMR2;    // on TMR2 steps
  uint16 m_unAckEarliestTMR2;   // on TMR2 steps
  uint16 m_unAckLatestTMR2;     // on TMR2 steps
}DLL_TIMESLOT_TRANSMIT;  // 2 bytes alignment

typedef union
{
  DLL_TIMESLOT_RECEIVE m_stRx;
  DLL_TIMESLOT_TRANSMIT m_stTx;
}DLL_TIMESLOT_TEMPLATE; // 2 bytes alignment

// timeslot template
typedef struct
{
  uint16 m_unUID;   // range 1..32767; 1 & 2 reserved for joining  
  uint8  m_ucInfo;  // b7b6 - timeslot type
                    //      0 - recv; 1 - transm; 2,3 - reserved
                    // b5 - ACK Reference
                    //      0 - offset from end of incoming/transmited DPDU
                    //      1 - negative offset from end of timeslots
                    
                    //for Rx type
                    // b4 - respect boundary; 0 - not needed tp respect slot boundaries
                    //                        1 - slot boundaries must be respected
                    // b3b2b1b0 - reserved
                    
                    //for Tx type
                    // b4b3 - CCA mode; 0 - no CCA, 1 - CCA when energy above threshold
                    //                  2 - CCA when carier sense only, 3 - CCA when carrier sense and energy above threshold
                    // b2 - keep listening after the ACk/NACK received; 0 - no, 1 - yes
                    // b1b0 - reserved
  DLL_TIMESLOT_TEMPLATE m_utTemplate; // depends on type - 2 bytes alignment
}DLL_SMIB_TIMESLOT; //2 bytes alignment

//default UIDs for timeslot templates according with ISA100 draft
#define DEFAULT_RX_TEMPL_UID        1
#define DEFAULT_TX_TEMPL_UID        2
#define DEFAULT_DISCOVERY_TEMPL_UID 3

// Timeslot masks
#define DLL_MASK_TSTYPE               0xC0 
#define DLL_ROT_TSTYPE                6
#define DLL_MASK_TSACKREF             0x20
#define DLL_ROT_TSACKREF              5
#define DLL_MASK_TSBOUND              0x10
#define DLL_ROT_TSBOUND               4
#define DLL_MASK_TSCCA                0x18       
#define DLL_ROT_TSCCA                 3
#define DLL_MASK_TSKEEP               0x04        
#define DLL_ROT_TSKEEP                2
#define DLL_TX_TIMESLOT               0x40

// neighbor diagnostic structure
typedef struct
{
  // baseline diagnostic members  
  uint16  m_unRxDPDUNo;
  uint16  m_unTxSuccesNo;
  uint16  m_unTxFailedNo;
  uint16  m_unTxCCABackoffNo;
  uint16  m_unTxNackNo;
  
  // detailed clock diagnostic members
  int16   m_nClkBias;
  uint16  m_unClkCount;
  uint16  m_unClkTimeoutNo;
  uint16  m_unClkOutliersNo;
  // not ISA100 
  uint16  m_unUnicastTxCtr;         // attempted unicast transmissions

} DLL_NEIGH_DIAG_COUNTERS;

typedef struct
{  
  uint16  m_unUID;      
  uint8   m_ucStatisticsInfo;   // xxxxxxyz;
                                // y - flag that must be set for first update of m_lClockCorrectionEMA
                                // z - flag that must be set for first update of m_cEMARSSI
  uint8   m_ucDiagFlags;        // copy of neighbor entry ucInfo (b3b2 are of intrest)
    
  // baseline diagnostic members  (not included in counter struct)  
  int8    m_cRSSI;  
  uint8   m_ucRSQI; 
  int16   m_nClockSigma;
  
  DLL_NEIGH_DIAG_COUNTERS m_stDiag;

  uint16 m_unFromLastCommunication; // used to track the seconds elapsed from the last acknowledged communication with this neighbor (units in 1/4 seconds)
  
} DLL_SMIB_NEIGHBOR_DIAG;

// masks for neighbor diagnostic m_ucStatisticsInfo
#define DLL_MASK_RSSIINFO             0x01       
#define DLL_MASK_CLOCKCORRINFO        0x02

#define DIAG_STRUCT_STANDARD_SIZE    24  // as required in ISA1000, all counters al considered maximum

// neighbor
typedef struct
{  
  uint16  m_unUID;
  uint8   m_ucGroupCode;  // may associate a group code with a set of neighbours
  uint8   m_ucInfo;       // b7b6 - neighbour is clock source (0=no,1=sec,2=pref)
                          // b5b4 - nof graphs extended for this neighbor
                          // b3b2 - diagnostic level 
                          // b1   - link backlog 
                          // b0   - reserved
  uint16  m_aExtendedGraphs[3]; // for each entry
                                // b11..b4 - graph id
                                // b3 - last hop; indicates whether the neighbor shall be the last hop
                                // b2 - indicates whether to treat the neighbor as the preferred branch
                                // b1b0 - reserved    
  uint8   m_aEUI64[8];            
  uint16  m_unLinkBacklogIdx;   // index to a link that may be activated through DAUX
  uint8   m_ucLinkBacklogDur;   // duration of link activation; units of 1/4 s - according with DLL Changes   
  
  // non ISA100  
  uint8  m_ucExpBackoff;      
  uint16 m_unBackoffCnt;        // counter for exponential backoff support  
  uint16 m_unNACKBackoffTmr;    // timer in units of 1/4 seconds; if not zero, Tx to this neighbor must be ignored  
  DLL_SMIB_NEIGHBOR_DIAG * m_pstDiag;  // pointer to a neighbor diagnostic entry
  uint8  m_ucCommHistory;       // history of the last 8 communication attempts with this neighbor
  uint8  m_ucECNIgnoreTmr;      // timer in 1/4 seconds used to ignore whenever possible a congestioned neighbor  
  uint8  m_ucAlertTimeout;      // timer in seconds; initialized when a neighDiag alert is generated;
  uint8  m_ucBacklogLinkTimer;  // used to track if link backlog has been activated with this neighbour; (unit in 1/4 seconds)
}DLL_SMIB_NEIGHBOR; // 2 bytes alignment
// Neighbor masks
//m_ucInfo
#define DLL_MASK_NEICLKSRC            0xC0
#define DLL_ROT_NEICLKSRC             6
#define DLL_MASK_NEINOFGR             0x30        
#define DLL_ROT_NEINOFGR              4
#define DLL_MASK_NEIDIAG              0x0C
#define DLL_ROT_NEIDIAG               2
#define DLL_MASK_NEILINKBACKLOG       0x02
#define DLL_ROT_NEILINKBACKLOG        1

#define DLL_MASK_NEIDIAG_LINK         0x04
#define DLL_MASK_NEIDIAG_CLOCK        0x08

//m_aExtendedGraphs[index]
#define DLL_MASK_NEIGRUID             0xFFF0      
#define DLL_ROT_NEIGRUID              4
#define DLL_MASK_NEIGRVIRT            0x0008      
#define DLL_ROT_NEIGRVIRT             3
#define DLL_MASK_NEIGRPREFBR          0x0004      
#define DLL_ROT_NEIGRPREFBR           2

// neighbor diagnostics reset
typedef struct
{
  uint16 m_unUID;
  uint8  m_ucLevel; // b7..b4 - reserved
                    // b3b2   - selection of neighbor diagnostics to collect); use DLL_MASK_NEIDIAG
                    // b1b0   - reserved
}DLL_SMIB_NEIGH_DIAG_RST;

// superframe
typedef struct
{
  uint16 m_unUID;    
  uint16 m_unTsDur;       // timeslot length (duration) range 2^(-20) sec (realigned with TAI every 250 ms)
  uint16 m_unChIndex;     // selects selects hopping pattern from channels
  uint16 m_unSfPeriod;    // base number of timeslots in each superframe cycle
  uint16 m_unSfBirth;     // slot number where the first superframe cycle nominally started (related to TAI=0)  
  uint16 m_unChMap;       // eliminate certain channels from hop seq for spectrum mgm
  uint8  m_ucChBirth;  
  uint8  m_ucInfo;        // b7b6 - type of superframe: 0-baseline, 1-hop on link only, 
                          //                            2-randomize slow hop, 4-randomize superframe period                              
                          // b5b4b3b2 - priority to select among multiple available links
                          // b1       - channel map override flag
                          // b0       - idle flag
  int32  m_lIdleTimer;    // idle/wakeup timer for superframe 
  uint16 m_unChRate;
  uint8  m_ucRndSlots;    
  
  uint8  m_ucChSeqLen;    // precalculated no of hopping channels
  uint8  m_aChSeq[8];     // m_unChMap & chTable[m_unChIndex]
  
  /****************************************************************************/
  // actual offsets within superframe !
  uint8  m_ucCrtChOffset;
  uint16 m_unCrtOffset;
  uint16 m_unCrtSlowHopOffset;
}DLL_SMIB_SUPERFRAME; // 4 bytes alignment

// Superframe masks
#define DLL_MASK_SFTYPE           0xC0        // SfType
#define DLL_MASK_SFPRIORITY       0x3C        // superframe Priority
#define DLL_MASK_CH_MAP_OV        0x02        // channel map override
#define DLL_MASK_SFIDLE           0x01        // superframe idle flag

#define DLL_ROT_SFTYPE            6
#define DLL_ROT_SFPRIORITY        2
#define DLL_ROT_SFCHMAPOVRD       1

enum
{
  SF_TYPE_BASELINE          = 0,
  SF_TYPE_HOP_ON_LINK       = 1,
  SF_TYPE_RND_SLOW_HOP_DUR  = 2,
  SF_TYPE_RND_SF_PERIOD     = 3
}; // SUPERFRAME_TYPES

// superframe idle
typedef struct
{
  uint16  m_unUID;
  uint8   m_ucIdle;     // b7 - reserved
                        // b6 - idle used; will use DLL_MASK_SFIDLE
                        // b5...b0 - reserved
  int32   m_lIdleTimer;  
} DLL_SMIB_SF_IDLE;

// graph
typedef struct
{
  uint16 m_unUID;
  uint16 m_unMaxLifetime; // units of 1/4 seconds
  uint16 m_aNeighbors[7];
  uint8  m_ucNeighCount;
  uint8  m_ucQueue;       // b7 - preffered branch; indicates whether to treat the first listed neighbor as the preferred branch
                          // b6b5b4   - neighbor count
                          // b3b2b1b0 - allocates buffers in the message queue for messages that are being forwarded using this graph
}DLL_SMIB_GRAPH;  // 2 bytes alignment
//graph masks
#define DLL_MASK_GRPREF     0x80  
#define DLL_ROT_GRPREF      7
#define DLL_MASK_GRNEICNT   0x70
#define DLL_ROT_GRNEICNT    4
#define DLL_MASK_GRQUEUE    0x0F
#define DLL_ROT_GRQUEUE     0  

typedef struct
{
  uint16 m_unOffset;
  uint16 m_unNA;
}DLL_SCHEDULE_OFFSET; // 2 bytes alignment

typedef struct
{
  uint16 m_unOffset;
  uint16 m_unInterval;
}DLL_SCHEDULE_OFFSET_INTERVAL;  // 2 bytes alignment

typedef struct
{
  uint16 m_unFirst;
  uint16 m_unLast;
}DLL_SCHEDULE_RANGE;  // 2 bytes alignment


typedef union
{
  DLL_SCHEDULE_OFFSET m_aOffset;
  DLL_SCHEDULE_OFFSET_INTERVAL m_aOffsetInterval;
  DLL_SCHEDULE_RANGE m_aRange;
  uint32 m_ulBitmap;
}DLL_SCHEDULE;   //4 bytes alignment

// link
typedef struct
{
  uint16 m_unUID;       
  uint16 m_unSfIdx;     // superframe index
  uint8  m_mfLinkType;  // b7   - transmit
                        // b6   - receive
                        // b5   - exponential backoff
                        // b4   - idle
                        // b3b2 - discovery; 0=none, 1=advertisment, 2=burst adv, 3=solicitation
                        // b1   - JoinResponse  
                        // b0   - Adaptive Allowed  
  uint8  m_ucPriority;  // 0000xxxx
  uint16 m_unTemplate1;
  uint16 m_unTemplate2;
  uint8  m_mfTypeOptions;   // b7b6 - NeighborType; 0=null, 1=address, 2=group
                            // b5b4 - GraphType; 0=null, 1=specific graph, 2=matching graph id(priority access)
                            // b3b2 - ScheduleType; 0=offset only, 1=offset&interval, 2=range, 3=bitmap
                            // b1   - ChannelType; 0=offset null, 1=use offset to select channel
                            // b0   - PriorityType; 0 = base link priority on superframe
  uint8  m_ucChOffset;      // select channel based on offset from superframe's hop pattern

  uint16 m_unNeighbor;      // identifies neighbor or group
  uint16 m_unGraphId;       // identity of graph with exclusive or prioritized access to link  

  DLL_SCHEDULE m_aSchedule; // link schedule
  
  /* following members are not part of a dlmo.link entry */
  uint8 m_ucActiveTimer;      // units of 1/4 seconds; link is active when m_ucActiveTimer in non-zero

}DLL_SMIB_LINK; // 4 bytes alignment

//link masks
//m_mfLinkType
#define DLL_MASK_LNKTX      0x80 
#define DLL_ROT_LNKTX       7
#define DLL_MASK_LNKRX      0x40  
#define DLL_ROT_LNKRX       6
#define DLL_MASK_LNKEXP     0x20
#define DLL_ROT_LNKEXP      5
#define DLL_MASK_LNKIDLE    0x10
#define DLL_ROT_LNKIDLE     4
#define DLL_MASK_LNKDISC    0x0C
#define DLL_ROT_LNKDISC     2
#define DLL_MASK_LNKJOIN    0x02
#define DLL_ROT_LNKJOIN     1
#define DLL_MASK_LNKADAPT   0x01
#define DLL_ROT_LNKADAPT    0

#define DLL_LNKDISC_ADV     1
#define DLL_LNKDISC_BURST   2
#define DLL_LNKDISC_SOLICIT 3

// m_mfTypeOptions
#define DLL_MASK_LNKNEI    0xC0 
#define DLL_ROT_LNKNEI      6
#define DLL_LNK_NEIGHBOR_MASK   0x40
#define DLL_LNK_GROUP_MASK      0x80

#define DLL_MASK_LNKGR      0x30  
#define DLL_ROT_LNKGR       4
#define DLL_MASK_LNKSC      0x0C  
#define DLL_ROT_LNKSC       2
#define DLL_MASK_LNKCH      0x02  
#define DLL_ROT_LNKCH       1
#define DLL_MASK_LNKPR      0x01  
#define DLL_ROT_LNKPR       0

// route
typedef struct
{
  uint16 m_unUID;
  uint8  m_ucInfo;  //b7b6b5b4 - number of routes
                    //b3b2 - alternative; 
                    //      = 0 select this route if Selector matches ContractID and "m_unNwkSrcAddr" matches the endpoint SrcAddr(only for BBR)
                    //      = 1 select this route if Selector matches ContractID
                    //      = 2 select this route if Selector matches destination address
                    //      = 3 use this route as the default; Selector not transmitted
                    //b1b0 - reserved
  uint8  m_ucFwdLimit;  // initializaiton value for the forwarding limit in DPDUs that use this route
  uint16 m_aRoutes[MAX_DROUT_ROUTES_NO]; // entry starts with 0 -> unicast; with 1010 -> graph
  uint16 m_unSelector;  // depends on option attribute
#ifdef BACKBONE_SUPPORT
  SHORT_ADDR m_unNwkSrcAddr; //a contract BBR route is unique identified by the ContractID and Nwk source address
#endif 
} DLL_SMIB_ROUTE;       //2 bytes alignment

// Route masks
#define DLL_MASK_RTNO   0xF0
#define DLL_ROT_RTNO    4

#define DLL_ROT_RTALT             2
#define DLL_RTSEL_MASK            (3 << DLL_ROT_RTALT)

#define DLL_RTSEL_BBR_CONTRACT    (0 << DLL_ROT_RTALT)
#define DLL_RTSEL_DEV_CONTRACT    (1 << DLL_ROT_RTALT)
#define DLL_RTSEL_DSTADDR         (2 << DLL_ROT_RTALT)
#define DLL_RTSEL_DEFAULT         (3 << DLL_ROT_RTALT)

//smibs
typedef struct
{
  DLL_SMIB_CHANNEL m_stChannel;
}DLL_SMIB_ENTRY_CHANNEL;    //4 bytes alignment

typedef struct
{
  DLL_SMIB_TIMESLOT m_stTimeslot;
}DLL_SMIB_ENTRY_TIMESLOT;   //4 bytes alignment

typedef struct
{
  DLL_SMIB_NEIGHBOR m_stNeighbor;
}DLL_SMIB_ENTRY_NEIGHBOR;   //4 bytes alignment

typedef struct
{
  DLL_SMIB_NEIGHBOR_DIAG  m_stNeighDiag;
}DLL_SMIB_ENTRY_NEIGDIAG;   //4 bytes alignment

typedef struct
{
  DLL_SMIB_NEIGH_DIAG_RST   m_stNeighDiagRst;
}DLL_SMIB_ENTRY_NEIGH_DIAG_RST;   //4 bytes alignment

typedef struct
{
  DLL_SMIB_SUPERFRAME m_stSuperframe;
}DLL_SMIB_ENTRY_SUPERFRAME;   //4 bytes alignment

typedef struct
{
  DLL_SMIB_SF_IDLE m_stSfIdle;
}DLL_SMIB_ENTRY_IDLE_SF;   //4 bytes alignment

typedef struct
{
  DLL_SMIB_GRAPH   m_stGraph;
}DLL_SMIB_ENTRY_GRAPH;      //4 bytes alignment

typedef struct
{
  DLL_SMIB_LINK    m_stLink;
}DLL_SMIB_ENTRY_LINK;   //4 bytes alignment

typedef struct
{
  DLL_SMIB_ROUTE   m_stRoute;
}DLL_SMIB_ENTRY_ROUTE;      //4 bytes alignment


typedef struct
{
  uint8 m_mfDllJoinBackTimeout; //xxxxyyyy
                                //xxxx - maximum extent of exponential backoff on joining
                                //yyyy - timeout to receive a system manager response to a join request
  uint8 m_mfDllJoinLinksType;   //
                                //b7b6 - schedule type for the Join Request Tx link; range 0-2
                                //b5b4 - schedule type for the Join Response Rx link; range 0-2
                                //b3   - indicate if "m_stDllJoinAdvRx" is valid and transmited                           
                                //b2b1 - schedule type for the Advertisement Rx link; range 0-2                                  
                                //b0   - reserved     
  DLL_SCHEDULE m_stDllJoinTx;   //schedule for Join Request Tx link
  DLL_SCHEDULE m_stDllJoinRx;   //schedule for Join Response Rx link
  DLL_SCHEDULE m_stDllJoinAdvRx;//schedule for Advertisement Rx link
} DLL_MIB_ADV_JOIN_INFO;


#define DLL_MASK_JOIN_BACKOFF       0xf0
#define DLL_MASK_JOIN_TIMEOUT       0x0f

#define DLL_MASK_JOIN_TX_SCHEDULE   0xC0
#define DLL_MASK_JOIN_RX_SCHEDULE   0x30
#define DLL_MASK_ADV_RX_FLAG        0x08
#define DLL_MASK_ADV_RX_SCHEDULE    0x06

#define DLL_ROT_JOIN_TX_SCHD        0x06
#define DLL_ROT_JOIN_RX_SCHD        0x04
#define DLL_ROT_ADV_RX_SCHD         0x01

#define SEC_KEY_LEN             16

typedef struct
{
  uint16  m_unBitMask;
  uint16  m_unTargetID;
} DLL_MIB_SUBNET_FILTER;

typedef struct
{
  uint16  m_unQueueCapacity; 
  uint16  m_unChannelMap; 
  uint16  m_unAckTurnaround;
  uint16  m_unNeighDiagCapacity;
  uint8   m_ucClockAccuracy;   
  uint8   m_ucDLRoles;    
  uint8   m_ucOptions;
} DLL_MIB_DEV_CAPABILITIES;

typedef struct
{
  int16   m_nEnergyLife;
  uint16  m_unListenRate;
  uint16  m_unTransmitRate;
  uint16  m_unAdvRate;  
}DLL_MIB_ENERGY_DESIGN;

typedef struct
{
  uint8   m_ucCandidateNo;
  uint16  m_unCandidateAddr[DLL_MAX_CANDIDATE_NO];
  int8    m_cRSQI[DLL_MAX_CANDIDATE_NO];  
} DLL_MIB_CANDIDATES;

typedef struct
{
  int16   m_nRSSIThresh;
  uint8   m_ucRSSIAlphaLow;
  uint8   m_ucRSSIAlphaHigh;
  int16   m_nRSQIThresh;
  uint8   m_ucRSQIAlphaLow;
  uint8   m_ucRSQIAlphaHigh;
  int16   m_nClkBiasThresh;
  uint8   m_ucClkBiasAlphaLow;
  uint8   m_ucClkBiasAlphaHigh;  
} DLL_MIB_SMOOTH_FACTORS;

typedef struct
{
  uint16  m_unNeiMinUnicast;  
  uint16  m_unChanMinUnicast;
  uint8   m_ucEnabled;
  uint8   m_ucNeiErrThresh;  
  uint8   m_unNoAckThresh;
  uint8   m_ucCCABackoffThresh;
} DLL_MIB_ALERT_POLICY;

// channel diagnostic structure
typedef struct
{
  struct
  {
      uint16  m_unUnicastTxCtr;
      uint16  m_unNoAckCtr;
      uint16  m_unCCABackoffCtr;  
  } m_astCh[16];
  
} DLL_MIB_CHANNEL_DIAG;

#define DLL_MAX_PRIORITY_NO 16
typedef struct
{
  uint8 m_ucPriorityNo;
  uint8 m_aucPriority[DLL_MAX_PRIORITY_NO];
  uint8 m_aucQMax[DLL_MAX_PRIORITY_NO];  
} DLL_MIB_QUEUE_PRIORITY;

/*
typedef struct
{
  uint8 m_ucSolic;  //xxxyyyyz
                    //xxx - daux type; if 1 then solicitation DAUX
                    //yyyy - daux target count; nof target times provided for resp to adv
                    //z - daux subnet include flag;
} DLL_MIB_SOLIC_TEMPLATE;
*/
typedef uint8 DLL_MIB_SOLIC_TEMPLATE;
#define DLL_MASK_SOLICTYPE  0xE0
#define DLL_ROT_SOLICTYPE   5
#define DLL_MASK_SOLICCNT   0x1E
#define DLL_ROT_SOLICCNT    1
#define DLL_MASK_SOLICSUBID 0x01
#define DLL_ROT_SOLICSUBID  0

typedef struct
{
  int32   m_lTaiCorrection;
  uint32  m_ulTaiTimeToApply;  
} DLL_MIB_TAI_ADJUST;
 
typedef struct
{
  uint32  m_ulRadioSilence;      //radio silence timeout; units 1 s
  uint32  m_ulRadioSleep;        //radio sleep period; units 1 s
  uint16  m_unAdvSuperframe;      
  uint16  m_unClockStale;        //clock source timeout; units 1 s
  uint16  m_unClockExpire;       //clock expiration deadline; units 1 s
  uint16  m_unSubnetId;          //identifier of the subnet that the DL has joined or is attempting to join  
  uint16  m_unIdleChannels;      //radio channels that shall be idle
  uint16  m_unCountryCode;       //information about the device's regulatory environment
  uint16  m_unNackBackoffDur;    //duration of backoff after receiving a NACK; units 1/4 s
  int16   m_nEnergyLeft;         // info about battery (power supply)
  uint8   m_ucActScanHostFract;  //device's behavior as an active scanning host
  uint8   m_ucMaxDSDUSize;       //maximum number of octets that can be accommodated in a single DSDU
  uint8   m_ucDiscoveryAlert;    //control of NeighborDiscovery alert  
  uint8   m_ucLinkPriorityXmit;  //default priority for transmit links
  uint8   m_ucLinkPriorityRcv;   //default priority for receive links
  uint8   m_ucMaxBackoffExp;     //maximum backof exponent for retries      
  int8    m_cRadioTransmitPower; //radio's maximum output power level
  uint16  m_unMaxLifetime;       //maximum PDU lifetime; units 1/4 s ; specification requires useless uint16 datatype
  
  DLL_MIB_SOLIC_TEMPLATE  m_stSolicTemplate;  //template of DAUX subheader used for solicitations
  DLL_MIB_SUBNET_FILTER   m_stAdvFilter;      //filter used on incoming advertisments during neighbor discovery
  DLL_MIB_SUBNET_FILTER   m_stSolicFilter;    //filter used on incoming solicitations
  DLL_MIB_CANDIDATES      m_stCandidates;     //a list of candidate neighbors, discovered by the DL
  DLL_MIB_SMOOTH_FACTORS  m_stSmoothFactors;  //smoothing factors for diagnostics
  DLL_MIB_CHANNEL_DIAG    m_stChDiag;         //channel diagnostics
  DLL_MIB_QUEUE_PRIORITY  m_stQueuePriority;  //queue buffer capacity for specified priority level
  DLL_MIB_ADV_JOIN_INFO   m_stAdvJoinInfo;    //advertise join information
  DLL_MIB_TAI_ADJUST      m_stTAIAdjust;      //adjust TAI time at an instant that is scheduled by the sys mngr
  DLL_MIB_ALERT_POLICY    m_stAlertPolicy;    
  
} DLL_MIB_STRUCT;


/*==========================[ public defines ]===============================*/

typedef struct
{
  DLL_SMIB_ENTRY_CHANNEL m_aTable[DLL_MAX_CHANNELS_NO];
} DLL_CHANNELS_STRUCT;

typedef struct
{
  DLL_SMIB_ENTRY_TIMESLOT m_aTable[DLL_MAX_TIMESLOTS_NO];
} DLL_TIMESLOTS_STRUCT;

typedef struct
{
  DLL_SMIB_ENTRY_NEIGHBOR m_aTable[DLL_MAX_NEIGHBORS_NO];
} DLL_NEIGHBORS_STRUCT;

typedef struct
{
  DLL_SMIB_ENTRY_NEIGDIAG m_aTable[DLL_MAX_NEIGH_DIAG_NO];
} DLL_NEIGHDIAG_STRUCT;

typedef struct
{
  DLL_SMIB_ENTRY_SUPERFRAME m_aTable[DLL_MAX_SUPERFRAMES_NO];
} DLL_SUPERFRAMES_STRUCT;

typedef struct
{
  DLL_SMIB_ENTRY_GRAPH m_aTable[DLL_MAX_GRAPHS_NO];
} DLL_GRAPHS_STRUCT;

typedef struct
{
  DLL_SMIB_ENTRY_LINK m_aTable[DLL_MAX_LINKS_NO];
} DLL_LINKS_STRUCT;

typedef struct
{
  DLL_SMIB_ENTRY_ROUTE m_aTable[DLL_MAX_ROUTES_NO];
} DLL_ROUTES_STRUCT;

typedef struct
{
    uint8 m_ucLinkIdx;
    uint8 m_ucSuperframeIdx;
    uint8 m_ucTemplateIdx;
    uint8 m_ucNeighborIdx;
    uint8 m_ucPriority;
    uint8 m_ucLinkType;
} DLL_HASH_STRUCT;

typedef struct
{    
    DLL_MIB_STRUCT         m_stMIB;
    DLL_CHANNELS_STRUCT    m_stChannels;
    DLL_TIMESLOTS_STRUCT   m_stTimeslots;
    DLL_NEIGHBORS_STRUCT   m_stNeighbors;
    DLL_NEIGHDIAG_STRUCT   m_stNeighDiag;
    DLL_SUPERFRAMES_STRUCT m_stSuperframes;
    DLL_GRAPHS_STRUCT      m_stGraphs;
    DLL_LINKS_STRUCT       m_stLinks;
    DLL_ROUTES_STRUCT      m_stRoutes;
    
    uint8                  m_aSMIBTableRecNo[ SMIB_IDX_NO ];
    uint8                  m_ucIdleLinksFlag;
    uint8                  m_ucHashTableSize;
    DLL_HASH_STRUCT        m_aHashTable[ DLL_MAX_LINKS_NO*2 ];    
} DLL_SMIB_DATA_STRUCT;


/*==========================[ globals ]======================================*/


//SMIB structures
extern DLL_SMIB_DATA_STRUCT g_stDll;
extern uint32 g_ulDllTaiSeconds;

#define g_ucDllChannelsNo       (g_stDll.m_aSMIBTableRecNo[SMIB_IDX_DL_CH])
#define g_ucDllTimeslotsNo      (g_stDll.m_aSMIBTableRecNo[SMIB_IDX_DL_TS_TEMPL])
#define g_ucDllNeighborsNo      (g_stDll.m_aSMIBTableRecNo[SMIB_IDX_DL_NEIGH])
#define g_ucDllNeighDiagNo      (g_stDll.m_aSMIBTableRecNo[SMIB_IDX_DL_NEIGH_DIAG])
#define g_ucDllSuperframesNo    (g_stDll.m_aSMIBTableRecNo[SMIB_IDX_DL_SUPERFRAME])
#define g_ucDllGraphsNo         (g_stDll.m_aSMIBTableRecNo[SMIB_IDX_DL_GRAPH])
#define g_ucDllLinksNo          (g_stDll.m_aSMIBTableRecNo[SMIB_IDX_DL_LINK])
#define g_ucDllRoutesNo         (g_stDll.m_aSMIBTableRecNo[SMIB_IDX_DL_ROUTE])

#define g_aDllChannelsTable    (g_stDll.m_stChannels.m_aTable)
#define g_aDllTimeslotsTable   (g_stDll.m_stTimeslots.m_aTable)
#define g_aDllNeighborsTable   (g_stDll.m_stNeighbors.m_aTable)
#define g_aDllNeighDiagTable   (g_stDll.m_stNeighDiag.m_aTable)
#define g_aDllSuperframesTable (g_stDll.m_stSuperframes.m_aTable)
#define g_aDllGraphsTable      (g_stDll.m_stGraphs.m_aTable)
#define g_aDllLinksTable       (g_stDll.m_stLinks.m_aTable)
#define g_aDllRoutesTable      (g_stDll.m_stRoutes.m_aTable)

// dll management information base (MIB) attributes
#define g_ucDllActScanHostFract         (g_stDll.m_stMIB.m_ucActScanHostFract)
#define g_stDllAdvJoinInfo              (g_stDll.m_stMIB.m_stAdvJoinInfo)
#define g_unDllAdvSuperframe            (g_stDll.m_stMIB.m_unAdvSuperframe)
#define g_ucMaxDSDUSize                 (g_stDll.m_stMIB.m_ucMaxDSDUSize)
#define g_unDllSubnetId                 (g_stDll.m_stMIB.m_unSubnetId)
#define g_unClockStale                  (g_stDll.m_stMIB.m_unClockStale)
#define g_unClockExpire                 (g_stDll.m_stMIB.m_unClockExpire)
#define g_unNackBackoffDur              (g_stDll.m_stMIB.m_unNackBackoffDur)
#define g_ucLinkPriorityXmit            (g_stDll.m_stMIB.m_ucLinkPriorityXmit)
#define g_ucLinkPriorityRcv             (g_stDll.m_stMIB.m_ucLinkPriorityRcv)
#define g_unMaxLifetime                 (g_stDll.m_stMIB.m_unMaxLifetime)
#define g_ucDiscoveryAlert              (g_stDll.m_stMIB.m_ucDiscoveryAlert)
#define g_ucAlertPolicy                 (g_stDll.m_stMIB.m_stAlertPolicy)
#define g_ucMaxBackoffExp               (g_stDll.m_stMIB.m_ucMaxBackoffExp)
#define g_stSolicTemplate               (g_stDll.m_stMIB.m_stSolicTemplate) 
#define g_stAdvFilter                   (g_stDll.m_stMIB.m_stAdvFilter)
#define g_stSolicFilter                 (g_stDll.m_stMIB.m_stSolicFilter)
#define g_stCandidates                  (g_stDll.m_stMIB.m_stCandidates)
#define g_stSmoothFactors               (g_stDll.m_stMIB.m_stSmoothFactors)
#define g_stChDiag                      (g_stDll.m_stMIB.m_stChDiag)
#define g_stQueuePriority               (g_stDll.m_stMIB.m_stQueuePriority)
#define g_stTaiAdjust                   (g_stDll.m_stMIB.m_stTAIAdjust)
#define g_unIdleChannels                (g_stDll.m_stMIB.m_unIdleChannels)
#define g_ulRadioSilence                (g_stDll.m_stMIB.m_ulRadioSilence  )
#define g_ulRadioSleep                  (g_stDll.m_stMIB.m_ulRadioSleep)
#define g_cRadioTransmitPower           (g_stDll.m_stMIB.m_cRadioTransmitPower)
#define g_unCountryCode                 (g_stDll.m_stMIB.m_unCountryCode)
#define g_nEnergyLeft                   (g_stDll.m_stMIB.m_nEnergyLeft)

extern uint32 g_ulRadioSleepCounter;
extern uint8  g_ucDiscoveryAlertTimer;
extern uint8 g_aDlmeMsgQueue[DLME_MSG_QUEUE_LENGTH]; // DLME message queue

#ifdef DLME_QUEUE_PROTECTION_ENABLED
   extern uint8  g_ucDLMEQueueBusyFlag;
   extern uint8  g_ucDLMEQueueCmd;
#endif

extern uint8  g_ucHashRefreshFlag;   

typedef struct
{
  uint16 m_unSrcAddr;
  uint16 m_unDstAddr;
  uint8  m_ucHopLeft;
} DLL_LOOP_DETECTED;

extern DLL_LOOP_DETECTED g_stLoopDetected;

typedef struct
{
  union
  {
    DLL_SMIB_CHANNEL        m_stChannel;
    DLL_SMIB_TIMESLOT       m_stTimeslot;
    DLL_SMIB_NEIGHBOR       m_stNeighbor;
    DLL_SMIB_NEIGH_DIAG_RST m_stNeighDiagReset;
    DLL_SMIB_SUPERFRAME     m_stSuperframe;
    DLL_SMIB_SF_IDLE        m_stSfIdle;
    DLL_SMIB_GRAPH          m_stGraph;
    DLL_SMIB_LINK           m_stLink;
    DLL_SMIB_ROUTE          m_stRoute;    
    DLL_SMIB_NEIGHBOR_DIAG  m_stNeighDiag;
  } m_stSmib;    
}DLMO_SMIB_ENTRY;

typedef struct
{
  uint16          m_unAlign;
  uint16          m_unUID;
  DLMO_SMIB_ENTRY m_stEntry;  
  
} DLMO_DOUBLE_IDX_SMIB_ENTRY;

/*==========================[ public functions ]=============================*/

void DLME_Init();

__monitor uint8 DLME_GetMIBRequest(uint8 p_ucAttrID, 
                         void* p_pValue);

__monitor uint8 DLME_SetMIBRequest(uint8       p_ucAttrID, 
                         const void* p_pValue);

__monitor uint8 DLME_GetSMIBRequest(uint8   p_ucAttrID, 
                          uint16  p_unUID, 
                          void*   p_pValue);

uint8 DLME_SetSMIBRequest(uint8       p_ucAttrID, 
                             uint32 p_ulSchedTAI,
                              const DLMO_DOUBLE_IDX_SMIB_ENTRY * pDoubleIdxEntry);

uint8 DLME_DeleteSMIBRequest(uint8 p_ucAttrID, 
                             uint16 p_unUID,
                             uint32 p_ulSchedTAI);

uint8 DLME_ResetSMIBRequest(uint8 p_ucTable, 
                            uint8 p_ucLevel,  
                            uint8 p_bNoRef);

uint8 DLME_GetMetaData(uint16   p_unAttrID,
                       uint16*  p_punSize,
                       uint8*   p_pBuf);

uint8* DLME_FindSMIB(LOCAL_SMIB_IDX   p_ucLocalID, uint16  p_unID);
uint8 DLME_FindSMIBIdx(LOCAL_SMIB_IDX  p_ucLocalID, uint16 p_unID);

uint8 MLSM_GetChFromTable( const uint8 * p_pChTable, uint8 p_ucChIdx );

void DLME_CopyReversedEUI64Addr( uint8 * p_pDst, const uint8 * p_pSrc );

// Message queue parser
void DLME_ParseMsgQueue( void );
void DLME_RebuildHashTable(void);

void  DLME_CheckNeighborDiscoveryAlarm(void);
uint8 DLME_CheckChannelDiagAlarm(void);
uint8 DLME_CheckNeighborDiagAlarm(void);

uint8 DLME_GetStrongestNeighbor( void );



#endif /* _NIVIS_DLME_H_ */

