# W3.4 - Kich ban GDB dieu tra hop den w3e4.elf.
# Chi doc RAM/thanh ghi. Khong sua .elf, khong goi ham tren target, khong ghi RAM board.
#
# Cach chay (tu goc repo, board + ST-Link da cam):
#   Terminal 1: openocd -f interface/stlink.cfg -f target/stm32f1x.cfg
#   Terminal 2: arm-none-eabi-gdb -nx -batch -x week3/ex4/debug_w3e4.gdb week3/ex4/w3e4.elf
# Doi thoi diem halt (ms sau reset): them  -ex "set $run_ms = 5000"  TRUOC -x.
# Log nguyen van (lenh + output): week3/ex4/w3e4-gdb.log
#
# Dia chi object lay tu symbol/DWARF cua chinh ELF nay (TCB_t 92 byte, List_t 20 byte, Queue_t 80 byte).
# So co dinh duy nhat: cua so PARAMS 0x20003300 va thanh ghi he thong Cortex-M3.

set pagination off
set confirm off
set language c
set print pretty on
set stack-cache off
set code-cache off
set remotetimeout 30

if $_isvoid($run_ms)
  set $run_ms = 2000
end

# ---------------------------------------------------------------- helper
# w3e4_walk <dia chi List_t> <offset cua ListItem trong TCB: 4 = state, 24 = event>
define w3e4_walk
  set trace-commands off
  if (unsigned int)$arg0 == 0
    printf "list pointer = NULL\n"
  else
    set $wl = (List_t *)$arg0
    set $wend = (unsigned int)&$wl->xListEnd
    set $wn = (unsigned int)$wl->xListEnd.pxNext
    set $wi = 0
    printf "list=%#x uxNumberOfItems=%u pxIndex=%#x sentinel=%#x\n", (unsigned int)$wl, $wl->uxNumberOfItems, (unsigned int)$wl->pxIndex, $wend
    while $wn != $wend && $wn != 0 && $wi < 16
      set $wit = (ListItem_t *)$wn
      set $wt = (TCB_t *)$wit->pvOwner
      printf "  node=%#x xItemValue=%#x pvOwner=%#x pvContainer=%#x", $wn, $wit->xItemValue, (unsigned int)$wt, (unsigned int)$wit->pvContainer
      if (unsigned int)$wt + $arg1 == $wn
        printf " -> task \"%s\" uxPriority=%u\n", $wt->pcTaskName, $wt->uxPriority
      else
        printf " -> owner/offset khong khop\n"
      end
      set $wn = (unsigned int)$wit->pxNext
      set $wi = $wi + 1
    end
    printf "  visited=%u declared=%u\n", $wi, $wl->uxNumberOfItems
  end
end

# w3e4_task <dia chi TCB> <so byte stack cap phat (sizeof symbol stack)>
define w3e4_task
  set trace-commands off
  set $t = (TCB_t *)$arg0
  set $sbytes = $arg1
  set $slow = (unsigned int)$t->pxStack
  set $stop = (unsigned int)$t->pxTopOfStack
  printf "TCB=%#x name=\"%s\" uxPriority=%u uxBasePriority=%u uxMutexesHeld=%u\n", (unsigned int)$t, $t->pcTaskName, $t->uxPriority, $t->uxBasePriority, $t->uxMutexesHeld
  printf "  xStateListItem.pvContainer=%#x : ", (unsigned int)$t->xStateListItem.pvContainer
  info symbol $t->xStateListItem.pvContainer
  printf "  xEventListItem.pvContainer=%#x : ", (unsigned int)$t->xEventListItem.pvContainer
  info symbol $t->xEventListItem.pvContainer
  if $slow == 0
    printf "  pxStack = NULL (task chua duoc tao)\n"
  else
    printf "  pxStack=%#x limit=%#x pxTopOfStack=%#x con_trong_luc_luu_ngu_canh=%d byte\n", $slow, $slow + $sbytes, $stop, $stop - $slow
    printf "  (gdb) x/16xw %#x\n", $slow
    x/16xw $slow
    set $k = 0
    while $k < $sbytes / 4 && *(unsigned int *)($slow + 4 * $k) == 0xa5a5a5a5
      set $k = $k + 1
    end
    printf "  watermark: %u word 0xA5A5A5A5 lien tuc tu day = %u byte chua tung bi ghi\n", $k, 4 * $k
  end
end

# w3e4_mutex <gia tri handle mutex = dia chi Queue_t>
define w3e4_mutex
  set trace-commands off
  if (unsigned int)$arg0 == 0
    printf "handle = NULL\n"
  else
    set $q = (Queue_t *)$arg0
    printf "Queue_t=%#x : ", (unsigned int)$q
    info symbol $q
    printf "  pcHead(+0)=%#x pcWriteTo(+4)=%#x xMutexHolder(+8)=%#x uxRecursiveCallCount(+12)=%u\n", (unsigned int)$q->pcHead, (unsigned int)$q->pcWriteTo, (unsigned int)$q->u.xSemaphore.xMutexHolder, $q->u.xSemaphore.uxRecursiveCallCount
    printf "  uxMessagesWaiting(+56)=%u uxLength(+60)=%u uxItemSize(+64)=%u ucQueueType(+76)=%u\n", $q->uxMessagesWaiting, $q->uxLength, $q->uxItemSize, $q->ucQueueType
    printf "  (gdb) x/20xw %#x\n", (unsigned int)$q
    x/20xw $q
    if $q->u.xSemaphore.xMutexHolder != 0
      printf "  holder -> task \"%s\" uxMutexesHeld=%u\n", $q->u.xSemaphore.xMutexHolder->pcTaskName, $q->u.xSemaphore.xMutexHolder->uxMutexesHeld
    else
      printf "  holder = NULL (mutex dang tu do)\n"
    end
    printf "  xTasksWaitingToReceive(+36):\n"
    set $wlist = (unsigned int)&$q->xTasksWaitingToReceive
    w3e4_walk $wlist 24
  end
end

# Anh chup ngan de so sanh hai lan halt (co tien trien hay khong).
define w3e4_brief
  p xTickCount
  p/x $pc
  if pxCurrentTCB != 0
    p pxCurrentTCB->pcTaskName
  end
  p/x ((TCB_t *)&s_task_a_tcb)->pxTopOfStack
  p/x ((TCB_t *)&s_task_b_tcb)->pxTopOfStack
  if s_mutex1 != 0
    p/x ((Queue_t *)s_mutex1)->u.xSemaphore.xMutexHolder
    p ((Queue_t *)s_mutex1)->xTasksWaitingToReceive.uxNumberOfItems
  end
  if s_mutex2 != 0
    p/x ((Queue_t *)s_mutex2)->u.xSemaphore.xMutexHolder
    p ((Queue_t *)s_mutex2)->xTasksWaitingToReceive.uxNumberOfItems
  end
  p xSuspendedTaskList.uxNumberOfItems
end

# ---------------------------------------------------------------- ket noi, nap, chay toi hien truong
# $w3e4_offline chi dung de kiem tra cu phap script khi khong co board.
if $_isvoid($w3e4_offline)
  set logging file week3/ex4/w3e4-gdb.log
  set logging overwrite on
  set logging enabled on
  set trace-commands on
  target extended-remote :3333
  monitor reset halt
  load
  compare-sections
  monitor reset halt
  # Bay loi: HardFault (= Default_Handler) va hook tran stack. Dia chi lay tu symbol.
  eval "monitor bp %#x 2 hw", (unsigned int)&Default_Handler
  eval "monitor bp %#x 2 hw", (unsigned int)&vApplicationStackOverflowHook
  monitor resume
  eval "monitor sleep %d", $run_ms
  monitor halt
  maintenance flush register-cache
else
  set trace-commands on
end

# ---------------------------------------------------------------- [L1] quet cua so SRAM trong PARAMS.md
echo \n===== [L1] x/512xw 0x20003300 (PARAMS: 0x20003300..0x20003AFF) =====\n
info symbol 0x20003300
info symbol 0x20003AFF
p/x &_ebss
p/x &_estack
if $_isvoid($w3e4_offline)
  x/512xw 0x20003300
end

# ---------------------------------------------------------------- [L2] CPU + task dang chay
echo \n===== [L2] CPU va task dang chay =====\n
if $_isvoid($w3e4_offline)
  info registers pc lr sp
  info all-registers
  x/6i $pc
  backtrace
end
p/x pxCurrentTCB
info symbol pxCurrentTCB
if pxCurrentTCB != 0
  p *pxCurrentTCB
  x/23xw pxCurrentTCB
end
p xTickCount
p uxCurrentNumberOfTasks
p uxTopReadyPriority
p xSchedulerRunning
p uxSchedulerSuspended

# ---------------------------------------------------------------- [L3] ready list theo tung priority (stride 20 byte)
echo \n===== [L3] pxReadyTasksLists[0 .. configMAX_PRIORITIES-1] =====\n
p sizeof(List_t)
p sizeof(pxReadyTasksLists)
set $nprio = sizeof(pxReadyTasksLists) / sizeof(List_t)
p $nprio
set $i = 0
set trace-commands off
while $i < $nprio
  set $la = (unsigned int)&pxReadyTasksLists[$i]
  printf "(gdb) x/5xw &pxReadyTasksLists[%u]    dia chi %#x = base + 20*%u\n", $i, $la, $i
  x/5xw $la
  w3e4_walk $la 4
  set $i = $i + 1
end
set trace-commands on

# ---------------------------------------------------------------- [L4] delayed / overflow / suspended / pending-ready
echo \n===== [L4] danh sach Blocked =====\n
p/x pxDelayedTaskList
set $la = (unsigned int)pxDelayedTaskList
w3e4_walk $la 4
set trace-commands on
p/x pxOverflowDelayedTaskList
set $la = (unsigned int)pxOverflowDelayedTaskList
w3e4_walk $la 4
set trace-commands on
x/5xw &xSuspendedTaskList
set $la = (unsigned int)&xSuspendedTaskList
w3e4_walk $la 4
set trace-commands on
x/5xw &xPendingReadyList
set $la = (unsigned int)&xPendingReadyList
w3e4_walk $la 24
set trace-commands on

# ---------------------------------------------------------------- [L5] gia thuyet 1: deadlock (holder / waiter)
echo \n===== [L5] mutex: holder (+8), wait list (+36), uxMutexesHeld (+80 trong TCB) =====\n
p/x s_mutex1
p/x s_mutex2
p &((Queue_t *)0)->u.xSemaphore.xMutexHolder
p &((Queue_t *)0)->xTasksWaitingToReceive
p &((TCB_t *)0)->uxMutexesHeld
w3e4_mutex s_mutex1
set trace-commands on
w3e4_mutex s_mutex2
set trace-commands on

# ---------------------------------------------------------------- [L6] gia thuyet 2: stack overflow (moi task, ke ca IDLE)
echo \n===== [L6] stack tung task: pxStack, pxTopOfStack, watermark 0xA5A5A5A5 =====\n
w3e4_task &s_task_a_tcb sizeof(s_task_a_stack)
set trace-commands on
w3e4_task &s_task_b_tcb sizeof(s_task_b_stack)
set trace-commands on
w3e4_task &s_idle_tcb sizeof(s_idle_stack)
set trace-commands on
w3e4_task &s_timer_tcb sizeof(s_timer_stack)
set trace-commands on
p xNumOfOverflows

# ---------------------------------------------------------------- [L7] gia thuyet 3: sai priority ISR
echo \n===== [L7] ICSR, NVIC, SHPR, BASEPRI, nguong syscall =====\n
if $_isvoid($w3e4_offline)
  x/xw 0xE000ED04
  p/u (*(unsigned int *)0xE000ED04) & 0x1ff
  x/2xw 0xE000E100
  x/32bx 0xE000E400
  x/28bx 0xE000E420
  x/3xw 0xE000ED18
  x/xw 0xE000ED0C
  p/x $basepri
  p/x $primask
  p/x $xpsr
  x/xw 0xE000ED28
  x/xw 0xE000ED2C
end
p/x ucMaxSysCallPriority
p/x ulMaxPRIGROUPValue
info functions FromISR
p sizeof(g_vector_table) / 4
x/59xw &g_vector_table
p/x &Default_Handler
p/x &HardFault_Handler

# ---------------------------------------------------------------- lan halt thu hai: he thong co tien trien khong
echo \n===== [S1] anh chup ngan, lan halt 1 =====\n
w3e4_brief
if $_isvoid($w3e4_offline)
  monitor resume
  eval "monitor sleep %d", $run_ms
  monitor halt
  maintenance flush register-cache
  echo \n===== [S2] anh chup ngan, lan halt 2 (sau them $run_ms ms) =====\n
  p $run_ms
  w3e4_brief
  set trace-commands off
  set logging enabled off
  disconnect
end
