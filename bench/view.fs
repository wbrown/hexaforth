: mark 30 emit ;
: size ( blk -- w h ) tiff-open dup image! tw @ th @ rot tiff-close ;
: own ( blk -- blk w h ) dup size ;
: half ( blk -- blk w h ) dup size 2/ swap 2/ swap ;
: alone? ( -- f ) up @ @ up @ = ;
: view ( blk w h -- ) view-load drop begin pause alone? until ;
