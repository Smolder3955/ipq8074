typedef unsigned int A_UINT32;
typedef unsigned long long A_UINT64;
typedef struct {
    A_UINT32 req_id; //unique request ID for this RTT measure req
    /******************************************************************************
     *bit 15:0       Request ID
     *bit 16:        sps enable  0- unenable  1--enable
     *bit 31:17      reserved
     ******************************************************************************/
    A_UINT32 loc_civic_params; // country code and length
    /******************************************************************************
     *bit 7:0        len in bytes. civic_info to be used in reference to this.
     *bit 31:8       reserved
     ******************************************************************************/
    A_UINT32 civic_info[64];      // Civic info including country_code to be copied in FTM frame.
                                  // 256 bytes max. Based on len, FW will copy byte-wise into
                                  // local buffers and transfer OTA. This is packed as a 4 bytes
                                  // aligned buffer at this interface for transfer to FW though.
} wmi_rtt_lcr_cfg_head;

typedef struct {
    A_UINT32 req_id; //unique request ID for this RTT measure req
    /******************************************************************************
     *bits 15:0       Request ID
     *bit  16:        sps enable  0- unenable  1--enable
     *bits 31:17      reserved
     ******************************************************************************/
    A_UINT64 latitude;             // LS 34 bits - latitude in degrees * 2^25 , 2's complement
    A_UINT64 longitude;            // LS 34 bits - latitude in degrees * 2^25 , 2's complement
    A_UINT32 lci_cfg_param_info;   // Uncertainities & motion pattern cfg
    /******************************************************************************
     *bits 7:0       Latitude_uncertainity as defined in Section 2.3.2 of IETF RFC 6225
     *bits 15:8      Longitude_uncertainity as defined in Section 2.3.2 of IETF RFC 6225
     *bits 18:16     Datum
     *bits 19:19     RegLocAgreement
     *bits 20:20     RegLocDSE
     *bits 21:21     Dependent State
     *bits 23:22     Version
     *bits 27:24     motion_pattern for use with z subelement cfg as per
                     wmi_rtt_z_subelem_motion_pattern
     *bits 30:28     Reserved
     *bits 31:31     Clear LCI - Force LCI to "Unknown"
      ******************************************************************************/
    A_UINT32 altitude;             // LS 30bits - Altitude in units of 1/256 m
    A_UINT32 altitude_info;
    /******************************************************************************
     *bits 7:0       Altitude_uncertainity as defined in Section 2.4.5 of IETF RFC 6225
     *bits 15:8      Altitude type
     *bits 31:16     Reserved
      ******************************************************************************/
     //Following elements for configuring the Z subelement
    A_UINT32  floor;               // in units 1/16th of floor # if known.
                                   // value is 80000000 if unknown.
    A_UINT32  floor_param_info;    // height_above_floor & uncertainity
    /******************************************************************************
     *bits 15:0      Height above floor in units of 1/64 m
     *bits 23:16     Height uncertainity as defined in 802.11REVmc D4.0 Z subelem format
                     value 0 means unknown, values 1-18 are valid and 19 and above are reserved
     *bits 31:24     reserved
      ******************************************************************************/
    A_UINT32  usage_rules;
    /******************************************************************************
     *bit  0         usage_rules: retransmittion allowed: 0-No 1-Yes
     *bit  1         usage_rules: retention expires relative present: 0-No 1-Yes
     *bit  2         usage_rules: STA Location policy for Additional neighbor info: 0-No 1-Yes
     *bits 7:3       usage_rules: reserved
     *bits 23:8      usage_rules: retention expires relative, if present, as per IETF RFC 4119
     *bits 31:24     reserved
      ******************************************************************************/
} wmi_rtt_lci_cfg_head;

