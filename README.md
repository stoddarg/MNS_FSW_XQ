- Version 1.0 This repository has been created to keep track of the FSW versions which I ship to ASU. They have boards with the XQ chip on them, which means they should be using this bitstream rather than the XC one. That bitstream will work (and has), but it would be ideal if they are using this one. This commit is what they were using to test some of the transfer function problems. 

- Version 1.1 This version includes a new detector number (0/1) for one of the sets of boot files. This is indicated in the folder name; these files have also been uploaded to dropbox for ease of use. Also, the baud rate for these boot files is lower (115200 bps) rather than the previous higher value. This is to try and just get a better transfer for lowering the baud with no other changes. A number of other changes have been implemented with this version, but they are documented starting on p97 of Graham's notes, book 102 (11). 

- Version 2.0 This version pulls in all the changes that have been made to the XC FSW repository for the Mini-NS as of 10/21. This project is to create the XQ boot files which are used by the Mini-NS Flight Unit and the Neutron-1 Flight Unit. This has boot files for version 4 of the FSW at both high and low baud rates.

- Version 2.1 Pulled in the updates from FSW_XC version 6.5. Added the 15ms wait into this version and updated the boot files in the FSBL folder.

--- to match up the version numbers, we are skipping ahead to the current FSW version ---

- Version 7.0 Bug fixes for the HV bug and the DIR checksum; implemented the DIR and DEL functions; other updates, see the main repository