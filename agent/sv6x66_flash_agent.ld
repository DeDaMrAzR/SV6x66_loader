ENTRY(SV6x66_FlashAgentEntry)

MEMORY
{
	ILM (rwx) : ORIGIN = 0x00004000, LENGTH = 0x0000B400
}

SECTIONS
{
	. = ORIGIN(ILM);
	.entry :
	{
		KEEP(*(.entry))
	} > ILM

	.text :
	{
		*(.text*)
		*(.rodata*)
	} > ILM

	.data :
	{
		*(.data*)
	} > ILM

	.bss (NOLOAD) :
	{
		__bss_start = .;
		*(.bss*)
		*(COMMON)
		__bss_end = .;
	} > ILM

	/DISCARD/ :
	{
		*(.comment*)
		*(.note*)
	}
}

ASSERT(SIZEOF(.entry) + SIZEOF(.text) + SIZEOF(.data) + SIZEOF(.bss) <= LENGTH(ILM), "RAM agent exceeds ILM")

