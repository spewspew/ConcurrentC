#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>

void
closekeyboard(Keyboardctl *kc)
{
	if(kc == nil)
		return;
	close(kc->ctlfd);
	close(kc->consfd);
	kc->consfd = kc->ctlfd = -1;
	threadint(kc->pid);
}

void
_ioproc(Keyboardctl *kc)
{
	int m, n, nerr;
	char buf[20];
	Rune r;

	threadsetname("kbdproc");
	n = 0;
	nerr = 0;
	while(kc->consfd >= 0){
		m = read(kc->consfd, buf+n, sizeof buf-n);
		if(m <= 0){
			yield();	/* if error is due to exiting, we'll exit here */
			if(kc->consfd < 0)
				break;
			fprint(2, "keyboard: short read: %r\n");
			if(m<0 || ++nerr>10)
				threadexits("read error");
			continue;
		}
		nerr = 0;
		n += m;
		while(n>0 && fullrune(buf, n)){
			m = chartorune(&r, buf);
			n -= m;
			memmove(buf, buf+m, n);
			if((kc->c @= r) < 0)
				break;
		}
	}
	chanfree(kc->c);
	free(kc->file);
	free(kc);
}

Keyboardctl*
initkeyboard(char *file)
{
	Keyboardctl *kc;
	char *t;

	kc = mallocz(sizeof(Keyboardctl), 1);
	if(kc == nil)
		return nil;
	if(file == nil)
		file = "/dev/cons";
	kc->file = strdup(file);
	kc->consfd = open(file, ORDWR|OCEXEC);
	t = malloc(strlen(file)+16);
	if(kc->consfd<0 || t==nil){
Error1:
		free(kc);
		return nil;
	}
	sprint(t, "%sctl", file);
	kc->ctlfd = open(t, OWRITE|OCEXEC);
	if(kc->ctlfd < 0){
		fprint(2, "initkeyboard: can't open %s: %r\n", t);
Error2:
		close(kc->consfd);
		free(t);
		goto Error1;
	}
	if(ctlkeyboard(kc, "rawon") < 0){
		fprint(2, "initkeyboard: can't turn on raw mode on %s: %r\n", t);
		close(kc->ctlfd);
		goto Error2;
	}
	free(t);
	chanset(kc->c, 20);
	kc->pid = coproc(_ioproc(kc), 4096);
	return kc;
}

int
ctlkeyboard(Keyboardctl *kc, char *m)
{
	return write(kc->ctlfd, m, strlen(m));
}
