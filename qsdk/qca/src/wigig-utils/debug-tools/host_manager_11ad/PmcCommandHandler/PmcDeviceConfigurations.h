/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef PMC_DEVICE_CONFIGURATIONS_H_
#define PMC_DEVICE_CONFIGURATIONS_H_

static const int DESCRIPTOR_SIZE_KB = 1;
static const int NUM_OF_DESCRIPTORS = 1024;

class PmcDeviceConfigurations
{
    public:
        PmcDeviceConfigurations()
            : m_collectIdleSmEvents(true)
            , m_collectRxPpduEvents(true)
            , m_collectTxPpduEvents(true)
            , m_collectUcodeEvents(true)
        {
        }

        bool IsCollectIdleSmEvents() const
        {
            return m_collectIdleSmEvents;
        }

        void SetCollectIdleSmEvents(bool collectIdleSmEvents)
        {
            m_collectIdleSmEvents = collectIdleSmEvents;
        }

        bool IsCollectRxPpduEvents() const
        {
            return m_collectRxPpduEvents;
        }

        void SetCollectRxPpduEvents(bool collectRxPpduEvents)
        {
            m_collectRxPpduEvents = collectRxPpduEvents;
        }

        bool IsCollectTxPpduEvents() const
        {
            return m_collectTxPpduEvents;
        }

        void SetCollectTxPpduEvents(bool collectTxPpduEvents)
        {
            m_collectTxPpduEvents = collectTxPpduEvents;
        }

        bool IsCollectUcodeEvents() const
        {
            return m_collectUcodeEvents;
        }

        void SetCollectUcodeEvents(bool collectUcodeEvents)
        {
            m_collectUcodeEvents = collectUcodeEvents;
        }

    private:

        bool m_collectIdleSmEvents;
        bool m_collectRxPpduEvents;
        bool m_collectTxPpduEvents;
        bool m_collectUcodeEvents;
};

inline std::ostream& operator<<(std::ostream& os, const PmcDeviceConfigurations& configurations)
{
    os << "Device Configurations: " << "Collect Idle Sm Events - " << configurations.IsCollectIdleSmEvents()
                                    << "Collect Rx Ppdu Events - " << configurations.IsCollectRxPpduEvents()
                                    << "Collect Tx Ppdu Events - " << configurations.IsCollectTxPpduEvents()
                                    << "Collect Ucode Events - " << configurations.IsCollectUcodeEvents();
    return os;
}

#endif /* PMC_DEVICE_CONFIGURATIONS_H_ */
