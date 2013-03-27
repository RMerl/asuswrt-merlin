c I am not sure whether there is a way to have a fortran program without
c a MAIN, but it does not really harm us to have one.
      end
      subroutine fsub
        integer*4 cppsub
        return (cppsub (10000))
      end
