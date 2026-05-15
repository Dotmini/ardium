// Topic: Advanced Global Scope
// Manage Global State (Unsafe Mutable Static)

static mut APP_STATUS: &str = "Active";
static mut APP_USERS: i32 = 0;

fn main() {
    unsafe {
        println!("Current Status: {}", APP_STATUS);
        
        APP_STATUS = "Maintenance";
        
        println!("New Status: {}", APP_STATUS);
    }
}
