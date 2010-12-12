rel Pos { pos: int }
rel Time { time: long }

pos: Pos;
time: Time;

rel Dims {
    min_time: long,
    max_time: long,
    min_pos: int,
    max_pos: int,
    avg_pos: real,
    add_pos: int
}

fn dim1(): Dims
{
    t := time extend(id = 1);
    p := pos extend(id = 1);

    st := (t, t project(id)) summarize(min_time = min(time, 0L),
                                       max_time = max(time, -3L));

    sp := (p, p project(id)) summarize(min_pos = min(pos, 0),
                                       max_pos = max(pos, 0),
                                       avg_pos = avg(pos, 0.0),
                                       add_pos = add(pos, 0));

    return (st * sp);
}

fn dim2(): Dims
{
    return time summarize(min_time = min(time, 0L), max_time = max(time, 0L))
           * pos summarize(min_pos = min(pos, 0), 
                           max_pos = max(pos, 0),
                           avg_pos = avg(pos, 0.0),
                           add_pos = add(pos, 0));
}

rel Queue {
    order: int,
}

queue: Queue;

fn next(): rel {order: int}
{
    return queue summarize(order = max(order, 0))
                 extend(norder = order + 1)
                 project(norder)
                 rename(order = norder);
}
