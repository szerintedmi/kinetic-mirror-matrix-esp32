## RTOS task structure

- **Own the stack size**: Capture stack/priority in one place per task so tuning does not require hunting through code.
- **Keep tasks single-purpose**: Split sensor acquisition, control loops, and connectivity; message across queues or task notifications to avoid hidden coupling.
- **Centralize task creation**: Instantiate tasks in `tasks/init_tasks.cpp` so boot order is obvious and staged demos can ignore optional tasks.
- **Use typed messages**: Define `struct` payloads for queues; no raw `void*` casts—prevents alignment bugs across MCU families.
- **Document shutdown behavior**: Even prototypes should support a graceful `TaskSuspend()` path for debugger resets and automated tests.
- **Pick a concurrency pattern per module**: Choose either shared mutexes or a single-owner task plus message passing; mixing models increases race risk.
- **Keep blocking bounded**: Favor finite timeouts over `portMAX_DELAY`, handle failures explicitly, and log contention so it surfaces during bring-up.
- **ISRs stay thin**: Use only the `*FromISR` APIs inside interrupt handlers and defer heavier work to tasks via queue or notification.
