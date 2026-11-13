//! Dynamic OCR code belongs in a non-elevated host, independently of feature settings.

#[cfg(windows)]
#[allow(non_snake_case)]
pub(crate) fn EnsureNativeHost() -> Result<(), String> {
    use std::ffi::c_void;
    #[link(name = "advapi32")]
    extern "system" {
        fn OpenProcessToken(Process: *mut c_void, Access: u32, Token: *mut *mut c_void) -> i32;
        fn GetTokenInformation(Token: *mut c_void, Class: u32, Data: *mut c_void, Size: u32, Length: *mut u32) -> i32;
        fn GetSidSubAuthorityCount(Sid: *mut c_void) -> *mut u8;
        fn GetSidSubAuthority(Sid: *mut c_void, Index: u32) -> *mut u32;
    }
    #[link(name = "kernel32")]
    extern "system" {
        fn GetCurrentProcess() -> *mut c_void;
        fn CloseHandle(Handle: *mut c_void) -> i32;
    }
    let Denied = || "OCR requires a non-elevated process".to_string();
    unsafe {
        let mut Token = std::ptr::null_mut();
        if OpenProcessToken(GetCurrentProcess(), 8, &mut Token) == 0 { return Err(Denied()); }
        let mut Elevated = 1u32;
        let mut Length = 0u32;
        let mut Allowed = GetTokenInformation(Token, 20, (&mut Elevated as *mut u32).cast(), 4, &mut Length) != 0 && Elevated == 0;
        GetTokenInformation(Token, 25, std::ptr::null_mut(), 0, &mut Length);
        // usize storage preserves TOKEN_MANDATORY_LABEL's pointer alignment on x64/ARM64.
        let mut Buffer = vec![0usize; (Length as usize).div_ceil(std::mem::size_of::<usize>())];
        if Length == 0 || GetTokenInformation(Token, 25, Buffer.as_mut_ptr().cast(), Length, &mut Length) == 0 {
            Allowed = false;
        } else {
            let Sid = *(Buffer.as_ptr() as *const *mut c_void);
            let Count = *GetSidSubAuthorityCount(Sid);
            Allowed = Allowed && Count > 0 && *GetSidSubAuthority(Sid, (Count - 1) as u32) <= 0x2000;
        }
        CloseHandle(Token);
        if Allowed { Ok(()) } else { Err(Denied()) }
    }
}

#[cfg(not(windows))]
#[allow(non_snake_case)]
pub(crate) fn EnsureNativeHost() -> Result<(), String> { Ok(()) }
