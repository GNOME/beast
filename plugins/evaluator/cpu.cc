// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html

#include "cpu.hh"

#include <algorithm>

using std::vector;
using std::max;
using namespace Bse::EvaluatorUtils;

CPU::CPU()
{
    regs = 0;
    n_registers = 0;
}

void CPU::set_program(const vector<Instruction>& new_instructions)
{
    if (regs)
	free (regs);

    instructions = new_instructions;
    /* alloc regs, initialize n_registers */

    n_registers = 0;
    vector<Instruction>::const_iterator ip;

    for(ip = instructions.begin(); ip != instructions.end(); ip++)
    {
	int r[4];
	ip->rw_registers(r[0],r[1],r[2],r[3]);

	for(int k = 0; k < 4; k++)
	    n_registers = max(r[k]+1, n_registers);
    }

    /* FIXME: */
    n_registers = max(2, n_registers);

    regs = (double *)calloc(n_registers, sizeof(double));
}

CPU::~CPU()
{
    if (regs)
	free(regs);
}

void CPU::print_registers(const Symbols& symbols)
{
    printf("STATE: n_registers = %d\n", n_registers);
    for(int i = 0; i < n_registers; i++)
	printf("  %8s = %.8g\n", symbols.name(i).c_str(), regs[i]);
}

void CPU::print_program(const Symbols& symbols)
{
    vector<Instruction>::const_iterator ip;
    for(ip = instructions.begin(); ip != instructions.end(); ip++)
	ip->print(symbols);
}

void CPU::execute()
{
    vector<Instruction>::const_iterator ip;
    for(ip = instructions.begin(); ip != instructions.end(); ip++)
	ip->exec(regs);
}

void CPU::execute_1_1_block(int sreg, int dreg, const float *sdata, float *ddata, int samples)
{
    assert(sreg >= 0 && sreg <= n_registers); /* g_return_if_fail */
    assert(dreg >= 0 && dreg <= n_registers); /* g_return_if_fail */

    for(int i = 0; i < samples; i++)
    {
	vector<Instruction>::const_iterator ip;
	regs[sreg] = sdata[i];
	for(ip = instructions.begin(); ip != instructions.end(); ip++)
	    ip->exec(regs);
	ddata[i] = regs[dreg];
    }
}

// vim:set ts=8 sts=4 sw=4:
