# lib68k_mem
Standalone Reference for validating Memory Functionality in lib68k

![image](https://github.com/user-attachments/assets/f265aea1-152c-4cf8-b390-8cdf80f235a6)

# Motive:

The following aims to act as a little, subsection of lib68k, to be able to validate and verify the efficiency and accuracy in terms of the Memory read and writes of my 68k Emulator.

This doesn't aim to be anything special outside of serving to provide aid in subsequent developments without breaking the main functionality of my stuff.

If my commits on lib68k are anything to go by, I have been scratching my head and wrestling with the Memory logic for as long as I can remember.
Therefore, this aims to put those quarms to rest


## Features:

Using the trace addressing space defined in lib68k, this isolated scheme allows for the validation of TRACE Level Addressing for handling pre-fetch

Returns proper TRACE evidence of the Memory R/W, how it communciates with other areas, fallthroughs for Buffer overflows, etc
