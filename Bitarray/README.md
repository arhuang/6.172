# Overview
Included are methods to shift the bits in an array, either by using byte level parallelism (8 bits at a time) or word level parallelism.
For example, if we wanted to shift/rotate an 8 bit array 2 bits to the right, that would mean the 0 index bit would move to index 2, and the 7 index bit (last bit) would move to index 1.

The method used to perform a rotation of bits is double reversal. Basically if we split the array into two parts, a and b, 
where the first part is the portion of the array up to the offset (amount to shift), and the second part is the rest of the array,
then we reverse the bits inside a and b, then reverse the entire new array once more.

The objective of this project is to make the rotation fast by moving/operating on multiple bits at once, instead of going bit by bit.
The speedup achieved using was from 0.0029 seconds (2900 microseconds) using the naive single bit method, to 0.00004 seconds 
(40 microseconds) using word level parallelism (64 bits at a time).
