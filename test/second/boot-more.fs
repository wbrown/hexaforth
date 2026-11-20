\ boot-more.fs - the second disk's file of this label, which is
\ not the one that is read: the first disk has the label too
\ (test/boot-more.fs), and a label is looked for on the first
\ disk first.  What this makes of `more` is not what that does.

header more : more-of ( -- n ) 99 ;
