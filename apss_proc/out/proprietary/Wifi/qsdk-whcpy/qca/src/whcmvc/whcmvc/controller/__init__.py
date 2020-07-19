#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""Controller infrastructure for the Whole Home Coverage demo

The following modules are exported:

    :mod:`console`
        manages interaction with the target device's shell

    :mod:`executor`
        handles periodic and asynchronous task execution

    :mod:`condition_monitor`
        defines a single periodic task to execute

    :mod:`command`
        set of asynchronous tasks used by the orchestrator to perform its
        desired work
"""

__all__ = ['console', 'executor', 'condition_monitor', 'command']
