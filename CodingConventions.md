## Calling Conventions ##

Functions organize their parameters in the following order:

  * OUT parameters
  * IN parameters
  * `pc_pool_t` parameter(s)


## Function Conventions ##

Each "object" in PoCore (typically) has the following functions:

`pc_type_create(*ob, ..., pool)`
> Create the object, allocated from `pool`, returning the result in `ob`.

`pc_type_destroy(ob)`
> Destroy `ob`. This will close external resources, but associated memory will remain allocated until the pool is destroyed.

`pc_type_track(ob, ctx)`
> Begin tracking `ob` within the tracking system managed by `ctx`.

`pc_type_track_via(ob, pool)`
> Begin tracking `ob` within the tracking system managed by the context implied by `pool`

`pc_type_owns(ob, pool)`
> Record that `ob` is an owner of `pool`. Tracking will be started for `ob` and `pool` if they are not already subject to tracking.

_note: the typical tracking functions are still subject to change_

## Log Messages ##

The PoCore projects uses [Subversion's log message guidelines](http://subversion.apache.org/docs/community-guide/conventions.html#log-messages).

## Structure Naming ##

All structures should be tagged with `pc_type_s`.

If the structure is referenced in a public header, then it should have a corresponding typedef named `pc_type_t`.

Private structures should not have a typedef, but will always be referred to with `struct pc_type_s`.