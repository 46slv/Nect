// Open the same disclosure a user uses before operating advanced typography.
export async function openMoreTypography(page) {
  const section = page.locator('.inspector-body details[data-text-inspector-section="typography"]');
  if (!(await section.evaluate((el) => el.open))) await section.locator('summary').click();
  await page.waitForFunction(
    () =>
      document.querySelector('.inspector-body details[data-text-inspector-section="typography"]')
        ?.open,
  );
}
