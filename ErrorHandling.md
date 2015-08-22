## Basics ##

Structured errors are returned by a number of PoCore functions. These errors are represented by `pc_error_t`, and are managed within a single PoCore context (`pc_context_t`).

Every error also stores the file and line number where it was created. This can help with debugging, by pointing to where the problem arose.

## Handled vs Unhandled ##

When an error is created, it is stored within the PoCore context and marked as "unhandled". The error is then usually returned to the function's caller. The caller can further propagate the error back to its caller (also see `pc_error_trace()`), or it may process the error in some way. When an error is processed, it should be marked as "handled" by calling `pc_error_handled(error)`. The error will be removed from the context and its memory will be freed.

It is important to note: unhandled errors represent **programmer errors**. An application should handle **all** errors returned by PoCore; at the minimum, to ignore them and call `pc_error_handled()`.

At termination, or at periodic intervals in a long-running process, an application can use `pc_context_unhandled()` to investigate any errors that were not handled. It may log these or clear them out by using `pc_error_handled()` on them.

When `PC_DEBUG` is defined, `pc_context_destroy(ctx)` will execute `abort()` if there are unhandled errors. You can then inspect the core dump and its queue of errors to determine where you may have missed handling an error return.

## Chaining Errors ##

Errors have optional links to an "original" error, forming a chain of errors. These chains are intended to provide supporting context to the original error. An example chain from a version control tool might look like:

  1. Could not commit working copy A
  1. Unable to process changes for A/B/file
  1. Unknown working copy state for A/B/file
  1. Integrity constraint violated, missing data for A/B/file

For error 1, its original error links to error 2 whose original error is error 3, and so on. `pc_error_original()` can be used to trace these links.

In addition, each error can also link to a "separate" error, which can also form its own chain. A separate error is a problem that occurred during processing, but is not directly part of the contextual chain of original errors. For example, using indentation, we can demonstrate some hypothetical errors for the above chain:

  1. Could not commit working copy A
    1. Problem cleaning up commit plain
    1. Could not close temporary file T
  1. Unable to process changes for A/B/file
  1. Unknown working copy state for A/B/file
    1. Server was not notified of problem state
    1. Connection abruptly closed
  1. Integrity constraint violated, missing data for A/B/file

The `pc_error_separate()` function can be used to see if an error has an associated separate error.

## Tracing ##

The `pc_error_trace()` function will insert "tracing" errors into chains. These tracing errors help to identify the call stack which caused a problem. An example error chain might then look like:

  1. tracing, foo.c:203
  1. tracing, foo.c:89
  1. Unable to process BAR, baz.c:1460
  1. tracing, glom.c:254
  1. tracing, glom.c:50
  1. Error reading BAR, file.c:190

Tracing errors are normally skipped in the `pc_error_t` query functions. To see these tracing errors, the `pc_error_info()` function must be used to fetch complete details on each error.

## Error Code Mapping ##

_TBD_