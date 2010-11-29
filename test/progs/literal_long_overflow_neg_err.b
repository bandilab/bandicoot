rel T { v: long }

t: T;

fn test(): T
{
    return t select(v > -9223372036854775809L);
}
