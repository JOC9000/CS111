#!/usr/bin/env python3
# NAME: Jorge Contreras, Nevin Liang
# EMAIL: jorgec9000@g.ucla.edu, nliang868@g.ucla.edu
# ID: 205379811, 705575353
import sys

inconsistency_toggle = 0

if len(sys.argv) != 2:
    if len(sys.argv) < 2:
        sys.stderr.write("Too few arguments passed")
    if len(sys.argv) > 2:
        sys.stderr.write("Too many arguments passed")
    sys.exit(1)

if ".csv" not in sys.argv[1]:
    sys.stderr.write("Incorrect file given, a .csv file is required")
    sys.exit(1)

f = open(sys.argv[1], "r")
f1 = f.readlines()

super_line = f1[0].split(",")
max_blocks = int(super_line[1])
max_inodes = int(super_line[6])
inode_size = int(super_line[4])
block_size = int(super_line[3])

num_inode_blocks = int(max_inodes * inode_size / block_size)

# boot block, SB, and group block always used
reserved_block_bitmap = [0, 1, 2]
group_line = f1[1].split(",")

# free block bitmap block
reserved_block_bitmap.append(int(group_line[6]))
# free inode bitmap block
reserved_block_bitmap.append(int(group_line[7]))
# include all inode_blocks as reserved
for i in range(num_inode_blocks):
    reserved_block_bitmap.append(int(group_line[8]) + i)

orig_blocks = []
for i in range(max_blocks):
    orig_blocks.append(0)
    orig_blocks[-1] = (i, "USED")
    if i in reserved_block_bitmap:
        orig_blocks[-1] = (i, "RESERVED")

inode_blocks = []
for line in f1:
    if "BFREE" in line:
        bfree_line = line.split(",")
        orig_blocks[int(bfree_line[1])] = (int(bfree_line[1]), "FREE")

    if "INODE" in line:
        inode_line = line.split(",")
        if len(inode_line) > 12:
            for iterator in range(15):
                if int(inode_line[12 + iterator]) != 0:
                    inode_blocks.append(0)
                    indirect_string = ""
                    offset = iterator

                    if iterator == 12:
                        indirect_string = "INDIRECT "
                    elif iterator == 13:
                        offset = 268
                        indirect_string = "DOUBLE INDIRECT "
                    elif iterator == 14:
                        offset = 65804
                        indirect_string = "TRIPLE INDIRECT "
                    inode_blocks[-1] = (indirect_string + "BLOCK", int(inode_line[iterator + 12]), int(inode_line[1]),
                                        offset)
                    # word, block, inode, offset

    if "INDIRECT" in line:
        indirect_line = line.split(",")
        inode_blocks.append(list())
        indirect_string = ""

        if int(indirect_line[2]) == 1:
            indirect_string = "INDIRECT"
        elif int(indirect_line[2]) == 2:
            indirect_string = "DOUBLE INDIRECT"
        elif int(indirect_line[2]) == 3:
            indirect_string = "TRIPLE INDIRECT"

        inode_blocks[-1] = (indirect_string + " BLOCK", int(indirect_line[5]), int(indirect_line[1]),
                            int(indirect_line[3]))

my_block_bitmap = []
for i in range(max_blocks):
    my_block_bitmap.append(0)
    my_block_bitmap[-1] = (i, "FREE")
    if i in reserved_block_bitmap:
        my_block_bitmap[-1] = (i, "RESERVED")

duplicated_blocks_list = []
for block in inode_blocks:
    if block[1] < 0 or block[1] > max_blocks:
        print("INVALID " + block[0] + " " + str(block[1]) + " IN INODE " + str(block[2]) + " AT OFFSET " +
              str(block[3]))
        inconsistency_toggle = 1
    if block[1] in reserved_block_bitmap:
        print("RESERVED " + block[0] + " " + str(block[1]) + " IN INODE " + str(block[2]) + " AT OFFSET " +
              str(block[3]))
        inconsistency_toggle = 1
    if (block[1], "FREE") in orig_blocks:
        print("ALLOCATED " + block[0] + " " + str(block[1]) + " ON FREELIST")
        inconsistency_toggle = 1
    if 0 <= block[1] < max_blocks and (block[1], "USED") in my_block_bitmap:
        duplicated_blocks_list.append(block[1])
    if 0 <= block[1] < max_blocks:
        my_block_bitmap[block[1]] = (block[1], "USED")

for block in inode_blocks:
    if block[1] in duplicated_blocks_list:
        print("DUPLICATE " + block[0] + " " + str(block[1]) + " IN INODE " + str(block[2]) + " AT OFFSET " +
              str(block[3]))
        inconsistency_toggle = 1

for entry in my_block_bitmap:
    if entry[1] == "FREE" and (entry[0], "USED") in orig_blocks:
        print("UNREFERENCED BLOCK " + str(entry[0]))
        inconsistency_toggle = 1

# inode inspection...
first_nonreserved_inode = int(super_line[7])
orig_inode_bitmap = []
inodes_modes = [0] * max_inodes
for i in range(1, max_inodes + 1):
    orig_inode_bitmap.append(0)
    # 1 stands for used, 0 stands for freed
    orig_inode_bitmap[-1] = (i, 1)

for i in range(first_nonreserved_inode):
    inodes_modes[i] = 1

num_actual_links = [0] * max_inodes
link_count = [0] * max_inodes
directories = []
inode_dir = []
for line in f1:
    if "IFREE" in line:
        ifree_line = line.split(",")
        # if int(ifree_line[1]) >= first_nonreserved_inode:
        orig_inode_bitmap[int(ifree_line[1]) - 1] = (int(ifree_line[1]), 0)
    if "INODE" in line:
        inode_line = line.split(",")
        index = int(inode_line[1]) - 1
        inodes_modes[index] = int(inode_line[3])
        link_count[index] = int(inode_line[6])

        if inode_line[2] == "d":
            inode_dir.append(inode_line[1])
    if "DIRENT" in line:
        dirent_line = line.split(",")
        index = int(dirent_line[3]) - 1
        if 1 <= index <= max_inodes:
            num_actual_links[index] = num_actual_links[index] + 1
        directories.append(0)
        # (parent_inode, name, reference_inode)
        name = dirent_line[6]
        directories[-1] = (int(dirent_line[1]), name.replace("\n", ""), int(dirent_line[3]))

for inode in orig_inode_bitmap:
    cur = inode[0] - 1
    if inode[1] == 0 and inodes_modes[cur] != 0:
        print("ALLOCATED INODE " + str(inode[0]) + " ON FREELIST")
        inconsistency_toggle = 1
    elif inode[1] == 1 and inodes_modes[cur] == 0:
        print("UNALLOCATED INODE " + str(inode[0]) + " NOT ON FREELIST")
        inconsistency_toggle = 1

# now onto directory consistency
for num in range(max_inodes):
    if link_count[num] != num_actual_links[num] and link_count[num] != 0:
        print("INODE " + str(num + 1) + " HAS " + str(num_actual_links[num]) + " LINKS BUT LINKCOUNT IS " +
              str(link_count[num]))
        inconsistency_toggle = 1

# 2 always a child of 2
parent_child = [(2, 2)]
for inode in inode_dir:
    for line in f1:
        if "DIRENT" in line:
            dirent_line = line.split(",")
            name = dirent_line[6].replace("\n", "")
            if dirent_line[3] == inode and name != "'.'" and name != "'..'":
                parent_child.append(0)
                parent_child[-1] = (int(dirent_line[1]), int(inode))

for dir_entry in directories:
    if dir_entry[2] < 1 or dir_entry[2] > max_inodes:
        print("DIRECTORY INODE " + str(dir_entry[0]) + " NAME " + dir_entry[1] + " INVALID INODE " + str(dir_entry[2]))
        inconsistency_toggle = 1
    if (dir_entry[2], 0) in orig_inode_bitmap and inodes_modes[dir_entry[2]] == 0:
        print("DIRECTORY INODE " + str(dir_entry[0]) + " NAME " + dir_entry[1] + " UNALLOCATED INODE " + str(dir_entry[2]))
        inconsistency_toggle = 1
    if dir_entry[1] == "'.'" and dir_entry[0] != dir_entry[2]:
        print("DIRECTORY INODE " + str(dir_entry[0]) + " NAME '.' LINK TO INODE " + dir_entry[2] + " SHOULD BE " +
              dir_entry[0])
        inconsistency_toggle = 1
    if dir_entry[1] == "'..'" and (dir_entry[2], dir_entry[0]) not in parent_child:
        correct = (0, 0)
        for entry in inode_dir:
            if (int(entry), dir_entry[0])  in parent_child:
                correct = (int(entry), dir_entry[0])
        print("DIRECTORY INODE " + str(dir_entry[0]) + " NAME '..' LINK TO INODE " + str(dir_entry[2]) + " SHOULD BE " +
              str(correct[0]))
        inconsistency_toggle = 1

# left to implement: .. check!
if inconsistency_toggle == 1:
    sys.exit(2)
f.close()
