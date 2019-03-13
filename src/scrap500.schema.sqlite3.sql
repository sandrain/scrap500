--
-- scrap500.schema.sqlite3.sql
--

pragma foreign_keys = on;

begin transaction;

drop table if exists site;
drop table if exists system;
drop table if exists sysattr_name;
drop table if exists sysattr_val;
drop table if exists top500;

-- [table] site
create table site (
    id integer primary key not null,
    site_id integer not null,
    name text,
    url text,
    segment text,
    city text,
    country text,
    unique(site_id)
);

-- [table] system
create table system (
    id integer primary key not null,
    system_id integer not null,
    name text not null,
    summary text,
    manufacturer text,
    url text,
    unique(system_id)
);

-- [table] sysattr_name
create table sysattr_name (
    id integer primary key not null,
    name text not null,
    unique(name)
);

insert into sysattr_name(id, name) values (1, 'Cores');
insert into sysattr_name(id, name) values (2, 'Memory');
insert into sysattr_name(id, name) values (3, 'Processor');
insert into sysattr_name(id, name) values (4, 'Interconnect');
insert into sysattr_name(id, name) values (5, 'Linpack Performance (Rmax)');
insert into sysattr_name(id, name) values (6, 'Theoretical Peak (Rpeak)');
insert into sysattr_name(id, name) values (7, 'Nmax');
insert into sysattr_name(id, name) values (8, 'Nhalf');
insert into sysattr_name(id, name) values (9, 'HPCG [TFlop/s]');
insert into sysattr_name(id, name) values (10, 'Power');
insert into sysattr_name(id, name) values (11, 'Power Measurement Level');
insert into sysattr_name(id, name) values (12, 'Measured Cores');
insert into sysattr_name(id, name) values (13, 'Operating System');
insert into sysattr_name(id, name) values (14, 'Compiler');
insert into sysattr_name(id, name) values (15, 'Math Library');
insert into sysattr_name(id, name) values (16, 'MPI');

-- [table] sysattr_val
create table sysattr_val (
    id integer primary key not null,
    nid integer not null references sysattr_name(id),
    sval text,          -- text value or unit for rval
    rval real,
    unique(id, nid)
);

-- [table] top500
create table top500 (
    id integer primary key not null,
    time integer not null,
    rank integer not null,
    system_id integer not null references system(system_id),
    site_id integer not null references site(site_id),
    unique(time, rank, system_id)   -- there exist ties in rank
);

end transaction;

