Communication Library
---------------------

The communication library handles all low-level communication details between the device and OSD Clients. 
The device is usually the hardware on which Open SoC Debug is integrated. 
OSD Clients are libraries or applications using the functionality of the OSD Communication API to interact with a device, or to receive data (e.g. traces) from it.
The physical interface (i.e. low-level communication) is not implemented in the communication library. 
Instead, the API provides hooks to connect different physical communication libraries to it.

Every device connects to one communication library. 
To enable the use case of multiple tools (such as a run-control debugger like GDB and a trace debugger) talking to the same OSD-enabled device, the communication library supports multiple clients.

A note on byte ordering
^^^^^^^^^^^^^^^^^^^^^^^
OSD is designed around 16 bit words.
In the hardware and in communication with the device, these words are represented in big endian byte ordering.
However, in all OSD software libraries, the 16 bit words are in **native** byte ordering.
This enables using the data words as ``uint16_t`` without worrying about byte ordering.
If the native byte ordering of the system running the OSD software is not big endian (i.e. almost always), the byte ordering of all data exchanged between the device and the OSD software must be adjusted.

.. doxygengroup:: libosd-com
  :content-only:
