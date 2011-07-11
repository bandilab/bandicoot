rel point {
    x: real,
    y: real,
}

fn rel_param(p: point)
{
}

fn inline_rel_param1(p: rel {x: string})
{
}

fn inline_rel_param2(p: rel {x: real, y: real}): point
{
    return p;
}

fn prim_param(i: int, r: real, l: long, s: string)
{
}

fn param_comb_01(i: int, r: real, l: long, s: string, p: point)
{
}

fn param_comb_02(i: int, r: real, p: point, l: long, s: string)
{
}

fn param_comb_03(p: point, i: int, r: real, l: long, s: string)
{
}
