// v0.7 save integrity: a failed or short file write must not be reported to the
// caller as a successful save.
//
// The generated sub_82117B90 keeps its own success flag at 1 and only lowers it
// for failures that happen before the write. A failed or short write leaves the
// flag at 1, so the caller is told the profile was saved. The write result and
// the number of bytes actually written are re-checked here instead of editing
// the generated body: the guard runs after the original body and only changes a
// reported success into a reported failure.
//
// Guest contract of the original function: r3 = save image, r4 = image length,
// returned r3 = success. It zeroes the module-level byte counter at 0x826C362C
// on entry (lis r10,-32148 / addi r28,r10,13868) and the original write wrapper
// stores the number of bytes written there only when the write succeeded.
// Host hook over the generated weak alias: the always-original generated
// implementation is kept and still runs; direct and indirect calls both reach
// this entry.
extern "C" void __imp__sub_82117B90(PPCContext&,uint8_t*);
static constexpr uint32_t kDwg2SaveWrittenBytes = 0x826C362C;
extern "C" void sub_82117B90(PPCContext& ctx,uint8_t* base) {
  const uint32_t requested = ctx.r4.u32;
  __imp__sub_82117B90(ctx,base);
  if (ctx.r3.u32) {
    const uint32_t written = REX_LOAD_U32(kDwg2SaveWrittenBytes);
    if (written != requested) {
      REXLOG_ERROR("DWG2 save: original write accepted {} of {} bytes; reporting the save as failed",written,requested);
      ctx.r3.u64 = 0;
    }
  }
}
