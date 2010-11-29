rel T { i: int }

t: T;

fn test(): T
{
    return t select(i < 2147483648);
}
