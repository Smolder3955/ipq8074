#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""Handlers to update the model from other contexts.

This module exports the following:

    :class:`AssociatedStationsHandlerIface`
        interface for updating associated station information

    :class:`AssociatedStationsHandler`
        implementation that informs the model of associated stations
        updates

    :class:`UtilizationsHandlerIface`
        interface for updating utilization information

    :class:`UtilizationsHandler`
        implementation that informs the model of utilization updates

    :class:`ThroughputsHandlerIface`
        interface for updating throughput information

    :class:`ThroughputsHandler`
        implementation that informs the model of throughput updates
"""


class AssociatedStationsHandlerIface(object):

    """Interface for updating the associated station information.

    A single method is provided for other entities to update the
    model with associated station information.
    """

    def __init__(self):
        """Initialize a new associated stations handler."""
        pass

    def update_associated_stations(self, stations, ap_id):
        """Update the associated stations on the VAPs provided.

        :param dict stations: dictionary where the keys are the VAP
            identifiers and each value is a list consisting of tuples,
            with the first element in the tuple being a client device
            MAC address and the second entry being an RSSI in dB
        :param str ap_id: the ID of the AP whose info is contained in
            this update event

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Derived classes must implement this method")


class AssociatedStationsHandler(AssociatedStationsHandlerIface):

    """Handler that directly passes the STA info to the model.

    The model is responsible for performing the update in the
    appropriate context.
    """

    def __init__(self, model):
        """Initialize a new handler with the current model."""
        AssociatedStationsHandlerIface.__init__(self)

        self._model = model

    def update_associated_stations(self, stations, ap_id):
        """Update the associated stations on the VAPs provided.

        :param dict stations: dictionary where the keys are the VAP
            identifiers and each value is a list consisting of tuples,
            with the first element in the tuple being a client device
            MAC address and the second entry being an RSSI in dB
        :param str ap_id: the ID of the AP whose info is contained in
            this update event
        """
        self._model.update_associated_stations(stations, ap_id)


class UtilizationsHandlerIface(object):

    """Interface for updating the utilization information.

    A single method is provided for other entities to update the
    model with utilization information.
    """

    def __init__(self):
        """Initialize a new utilization handler"""
        pass

    def update_utilizations(self, utilizations, ap_id):
        """Update the utilizations on the VAPs provided

        :param dict utilizations: dictionary where the keys are the VAP
            identifiers and values are the utilizations of the VAP
        :param str ap_id: the ID of the AP whose info is contained in
            this update event

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Derived classes must implement this method")


class UtilizationsHandler(UtilizationsHandlerIface):

    """Handler that directly passes the utilization info to the model

    The model is responsible for performing the update in the
    appropriate context.
    """

    def __init__(self, model):
        """Initialize a new handler with the current model."""
        UtilizationsHandlerIface.__init__(self)

        self._model = model

    def update_utilizations(self, utilizations, ap_id):
        """:see UtilizationsHandlerIface.update_utilizations"""
        self._model.update_utilizations(utilizations, ap_id)


class ThroughputsHandlerIface(object):

    """Interface for updating the throughput information.

    A single method is provided for other entities to update the
    model with throughput information.
    """

    def __init__(self):
        """Initialize a new throughput handler"""
        pass

    def update_throughputs(self, througputs, ap_id):
        """Update the throughput on the VAPs provided

        Args:
            throughputs (dict): dictionary where the keys are the VAP
                identifiers and values are a tuple of the TX and RX
                throughputs of the VAP
            ap_id (str): the ID of the AP whose info is contained in
                this update event

        Raises:
            :exc:NotImplementedError
                This method must be implemented by derived classes.
        """
        raise NotImplementedError("Derived classes must implement this method")


class ThroughputsHandler(ThroughputsHandlerIface):

    """Handler that directly passes the throughput info to the model

    The model is responsible for performing the update in the
    appropriate context.
    """

    def __init__(self, model):
        """Initialize a new handler with the current model."""
        ThroughputsHandlerIface.__init__(self)

        self._model = model

    def update_throughputs(self, throughputs, ap_id):
        """:see ThroughputsHandlerIface.update_utilizations"""
        self._model.update_throughputs(throughputs, ap_id)

# Exports
__all__ = ['AssociatedStationsHandlerIface', 'AssociatedStationsHandler',
           'UtilizationsHandlerIface', 'UtilizationsHandler',
           'ThroughputsHandlerIface', 'ThroughputsHandler']
