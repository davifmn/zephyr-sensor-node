# The GDB session from the README, run by CI against QEMU (west build -t debugserver_qemu):
# a conditional breakpoint in a thread, then a breakpoint inside an interrupt handler.
set pagination off
set confirm off
set print pretty on
target remote localhost:1234

echo \n=== 1. Conditional breakpoint in the processing thread (the 5th reading)\n
break node_process if r->seq == 5
continue
print *r
backtrace

echo \n=== 2. The node state, read from memory\n
print 'node.c'::status.processed
print 'node.c'::alarm

echo \n=== 3. Breakpoint in interrupt context: the watchdog callback\n
delete
break watchdog_expired
continue
print (const char *)user_data
echo Active exception number (IPSR; 15 = SysTick, the system timer):\n
print/d $xpsr & 0x1ff
backtrace

kill
quit
