/* { dg-options "-DCE_EXTRACT_FUNCTIONS=f -DCE_NO_EXTERNALIZATION" }*/
/* { dg-skip-on-archs "s390x" }*/
#ifdef __x86_64__
register unsigned long current_stack_pointer asm("rsp");
#elif __i386__
register unsigned long current_stack_pointer asm("esp");
#elif __aarch64__
register unsigned long current_stack_pointer asm("sp");
#elif __PPC64__
register unsigned long current_stack_pointer asm("r1");
#endif

unsigned long f()
{
  return current_stack_pointer;
}

/* { dg-final { scan-tree-dump "current_stack_pointer asm" } } */
/* { dg-final { scan-tree-dump "unsigned long f" } } */
