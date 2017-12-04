# zlogan

## Description

High-throughput logic analyzer to be synthesized into an FPGA

* efficient and versatile encoding:
** RLE-encoding
** variable-length delta-time encoding
** ProtoBuf-compatible n*7-bit integers encoded direcly in HW at full clock speed
** assures re-synchronization after loss of some bytes during the transfer
* synchronous output interface
** FIFO-like data + strobe output
** to be connected to fast DMA interfaces (such as AXI4-Stream interface)
** 32-bit or 64-bit word length recommended
* output raw data convertible to VCD
** viewable with Dinotrace, GtkWave, Sigrok/PulseView
** protocol may be decoded using Sigrok/libsigrokdecode2 + .py plugins

Data format: each sample is encoded as ``(delta_time << n_wires) | wire_values`` in the form of ProtoBuf varint. The length of resulting varint is 1 to N bytes in case of N-byte output bus word length. Rationale: more frequent events have low delta_time, thus encoded to shorter words. Very sparse events with delta_time exceeding counter width (e.g. 24 bits) are encoded as subsequent samples with maximum delta_time and no changes in wire_values.

Rationale: the alternative would be to encode, which of the signals has changed, i.e. instead of copying all wire_values, just an index. However, for low wire counts the n_wires vs. log2(n_wires) savings is negligible; for high wire counts and situations, where many signals change at a time, would be also inefficient.

## Status

Tested within Zynq-7/Artix, up to 0.5GB uninterrupted captures, 200Msps.

## TODO

* support of falling edge sampling (2x sampling frequency)
* optional support of quadrature, DLL/PLL-based clock input (4x sampling frequency)
* Xilinx Vivado block design entity with parametric UI settings
* AXI-Lite slave for various settings: enable, wire change masks

## Author

Copyright (c) 2017 Marek Peca, Eltvor