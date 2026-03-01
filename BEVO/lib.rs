pub mod proto {
    include!(concat!(env!("OUT_DIR"), "/_.rs"));
}

pub mod config;
pub mod generated_mapping;

pub fn set_vec_index(v: &mut Vec<f32>, i: usize, val: f32) {
    if v.len() <= i { v.resize(i + 1, 0.0); }
    v[i] = val;
}