rel T {
    i: int,
    r: real,
    l: long,
}

fn test(t: T): T
{
    i_sel := t select(i < int(i))
               select(i < int(r))
               select(i < int(l));

    r_sel := t select(r < real(i))
               select(r < real(r))
               select(r < real(l));

    l_sel := t select(l < long(i))
               select(l < long(r))
               select(l < long(l));

    return i_sel + r_sel + l_sel;
}
