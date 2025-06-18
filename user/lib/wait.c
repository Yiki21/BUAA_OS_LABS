#include <env.h>
#include <lib.h>
int wait(u_int envid) {
	const volatile struct Env *e;

	e = &envs[ENVX(envid)];
	//debugf("wait for %d\n", envid);
	while (e->env_id == envid && e->env_status != ENV_FREE) {
		syscall_yield();
	}
	return e->ret_code;
}
