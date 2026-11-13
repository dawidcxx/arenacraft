import { useLayoutEffect, useRef, type RefObject } from "react";

// Rows are laid out normally; the animation is a FLIP: measure where each row settled
// (`offsetTop`, which ignores transforms), then play it from its previous position.
const MOVE_DURATION_MS = 420;
// A touch of overshoot makes a rank swap feel springy instead of mechanical.
const MOVE_EASING = "cubic-bezier(0.22, 1.12, 0.36, 1)";
const ENTER_DURATION_MS = 280;
const ENTER_STAGGER_MS = 28;
// Newcomers slide up from roughly the rank they just climbed from.
const ENTER_OFFSET_PX = 8;

/**
 * Animates row reordering inside a container of keyed rows (`data-row-key`).
 * Rows glide to their new spot; rows that were not there before fade/slide in.
 */
export function useRowMotion(
  containerRef: RefObject<HTMLElement | null>,
  players: readonly { name: string }[],
) {
  const previousTops = useRef(new Map<string, number>());

  useLayoutEffect(() => {
    const container = containerRef.current;
    if (container === null) return;

    const rows = [...container.querySelectorAll<HTMLElement>("[data-row-key]")];
    const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    const nextTops = new Map<string, number>();

    rows.forEach((row, index) => {
      const key = row.dataset.rowKey;
      if (key === undefined) return;

      const top = row.offsetTop;
      const previousTop = previousTops.current.get(key);
      nextTops.set(key, top);

      if (reduceMotion) return;

      if (previousTop === undefined) {
        // `fill: backwards` keeps the row hidden through its stagger delay.
        row.animate(
          [
            { opacity: 0, transform: `translateY(${ENTER_OFFSET_PX}px)` },
            { opacity: 1, transform: "translateY(0)" },
          ],
          {
            duration: ENTER_DURATION_MS,
            delay: index * ENTER_STAGGER_MS,
            easing: "ease-out",
            fill: "backwards",
          },
        );
        return;
      }

      const offset = previousTop - top;
      if (Math.abs(offset) < 1) return;

      row.animate([{ transform: `translateY(${offset}px)` }, { transform: "translateY(0)" }], {
        duration: MOVE_DURATION_MS,
        easing: MOVE_EASING,
      });
    });

    previousTops.current = nextTops;
  }, [containerRef, players]);
}