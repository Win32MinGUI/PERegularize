## PERegularize

#### A simple tool of regularizing PE32/PE32+ files according to my own favor

- FileHeader.TimeDateStamp is set to 0
- OptionalHeader.MajorLinkerVersion and OptionalHeader.MinorLinkerVersion are set to 0
- OptionalHeader.MajorOperatingSystemVersion is set to 4
- OptionalHeader.MinorOperatingSystemVersion is set to 0
- OptionalHeader.MajorSubsystemVersion is set to 4
- OptionalHeader.MinorSubsystemVersion is set to 0
- SizeOfRawData and PointerToRawData values in SectionHeaders are fit into FileAlignmentMask
- DLL names in the Import Table are capticalized and sorted by ascending order