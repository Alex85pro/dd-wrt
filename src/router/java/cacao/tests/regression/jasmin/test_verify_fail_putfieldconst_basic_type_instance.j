.class public test_verify_fail_putfieldconst_basic_type_instance
.super java/lang/Object

.field public intfield I

; ======================================================================

.method public <init>()V
   .limit stack 2

   aload_0
   invokenonvirtual java/lang/Object/<init>()V

   aload_0
   ldc 567
   putfield test_verify_fail_putfieldconst_basic_type_instance/intfield I

   return
.end method

; ======================================================================

.method public static check(I)V
	.limit locals 1
	.limit stack 10
	getstatic java/lang/System/out Ljava/io/PrintStream;
	iload_0
	invokevirtual java/io/PrintStream/println(I)V
	return
.end method

; ======================================================================

.method public static main([Ljava/lang/String;)V
	.limit stack 2
	.limit locals 2

	new test_verify_fail_putfieldconst_basic_type_instance
	dup
	invokespecial test_verify_fail_putfieldconst_basic_type_instance/<init>()V

	pop
	iconst_1

	iconst_2
	putfield test_verify_fail_putfieldconst_basic_type_instance/intfield I
	; ERROR: VerifyError
	; CACAOICMD: PUTFIELDCONST

	return
.end method

