\ apps.fs - the applications and their pages; a disk's block 1
meta 2 big-page  small-first target  include tiff.fs
meta 2 small-page target  include atkinson.fs
meta 1 big-page target  include text.fs
meta 1 small-page target  include term.fs
meta small-first target  include edit.fs
meta 1 small-page target  include see.fs
meta 2 sys-page target  include gray.fs
meta 3 big-page target  include view.fs
meta 0 small-page target  include calc.fs
meta 1 small-page target  include paint.fs
W64? [if]
include virtio.fs  include virtio-input.fs  include mandrill.fs
[then]
meta 2 small-page target  include desk.fs
