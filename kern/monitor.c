// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE 80 // enough for one VGA text line

struct Command
{
	const char *name;
	const char *desc;
	const char *usage;
	// return -1 to force monitor to exit
	int (*func)(int argc, char **argv, struct Trapframe *tf);
};

static struct Command commands[] = {
	{"help", "Display this list of commands or one of the command", "help\nhelp <command>", mon_help},
	{"kerninfo", "Display information about the kernel", "kerninfo", mon_kerninfo},
	{"backtrace", "Display backtrace info", "backtrace", mon_backtrace},
};
#define NCOMMANDS (sizeof(commands) / sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int mon_help(int argc, char **argv, struct Trapframe *tf)
{
	// cprintf("%x\n", UPAGES);
	if (argc == 1)
	{
		int i;
		for (i = 0; i < NCOMMANDS; i++)
			cprintf("%s - %s - usage:\n%s\n", commands[i].name, commands[i].desc, commands[i].usage);
	}
	else if(argc==2){
		cprintf("%s - %s - %s\n", commands[1].name, commands[1].desc, commands[1].usage);
	}
	else{
		cprintf("help usage:\n%s\n",commands[0].usage);
	}
	return 0;
}

int mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
			ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.

	uint32_t ebp, eip, arg[5];
	uint32_t *ptr_ebp;
	struct Eipdebuginfo info;

	ebp = read_ebp();
	eip = *((uint32_t *)ebp + 1);
	arg[0] = *((uint32_t *)ebp + 2);
	arg[1] = *((uint32_t *)ebp + 3);
	arg[2] = *((uint32_t *)ebp + 4);
	arg[3] = *((uint32_t *)ebp + 5);
	arg[4] = *((uint32_t *)ebp + 6);
	cprintf("Stack backtrace:\n");
	while (ebp != 0x00)
	{
		ptr_ebp = (uint32_t *)ebp;
		cprintf("ebp %08x eip %08x args %08x %08x %08x  %08x %08x\n",
				ebp, eip, arg[0], arg[1], arg[2], arg[3], arg[4]);
		ebp = *(uint32_t *)ebp;
		eip = *((uint32_t *)ebp + 1);
		arg[0] = *((uint32_t *)ebp + 2);
		arg[1] = *((uint32_t *)ebp + 3);
		arg[2] = *((uint32_t *)ebp + 4);
		arg[3] = *((uint32_t *)ebp + 5);
		arg[4] = *((uint32_t *)ebp + 6);
		if (debuginfo_eip(ptr_ebp[1], &info) == 0)
		{
			uint32_t fn_offset = ptr_ebp[1] - info.eip_fn_addr;
			cprintf("\t\t%s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, fn_offset);
		}
		ebp = *ptr_ebp;
	}
	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1)
	{
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS - 1)
		{
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++)
	{
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	while (1)
	{
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
