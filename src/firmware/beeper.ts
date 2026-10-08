import {
  contactsList,
  contactsReset,
  createContactBook,
  type ContactBook,
} from "./contacts";
import type { Hal, Page } from "./hal";
import {
  createIdentityStore,
  identityLoadOrCreate,
  identityReset,
  type IdentityStore,
} from "./identity";
import {
  createPageRadio,
  pageRadioReset,
  pageTick,
  type PageRadio,
} from "./page";
import {
  createPager,
  pagerGoMenu,
  pagerHandleText,
  pagerInput,
  pagerOnPage,
  pagerPollSend,
  pagerRender,
  pagerReset,
  type Pager,
} from "./pager";
import { createPairSession, pairActive, pairReset, pairTick, type PairSession } from "./pair";
import { storeLoad, storeSave, storeSync } from "./store";

export type Beeper = {
  identity: IdentityStore;
  contacts: ContactBook;
  pair: PairSession;
  radio: PageRadio;
  pager: Pager;
  nvmReady: boolean;
  nvmHash: number;
};

export function createBeeper(): Beeper {
  const identity = createIdentityStore();
  const contacts = createContactBook();
  const pair = createPairSession();
  const radio = createPageRadio();
  const pager = createPager(identity, contacts, pair, radio);
  return { identity, contacts, pair, radio, pager, nvmReady: false, nvmHash: 0 };
}

export function beeperTick(b: Beeper, hal: Hal): void {
  if (!b.nvmReady) {
    storeLoad(b, hal);
    b.nvmReady = true;
  }
  identityLoadOrCreate(b.identity, hal.randomFill);

  let gotPage = false;
  const nBefore = contactsList(b.contacts).length;
  if (pairActive(b.pair)) {
    pairTick(b.pair, b.identity, b.contacts, hal);
    if (!pairActive(b.pair) && contactsList(b.contacts).length > nBefore) {
      pagerGoMenu(b.pager);
    }
  } else {
    pageTick(b.radio, b.identity, b.contacts, hal, (p) => {
      pagerOnPage(b.pager, hal, p);
      gotPage = true;
    });
  }

  pagerPollSend(b.pager, hal);

  const incoming: Page = { from: "", body: "", rssi: 0, receivedMs: 0 };
  while (hal.radioPollRx(incoming)) {
    pagerOnPage(b.pager, hal, incoming);
    gotPage = true;
  }

  // A new page this tick: hold the alert; leave encoder/back queued.
  if (!gotPage) pagerInput(b.pager, hal);

  pagerRender(b.pager, hal);
  storeSync(b, hal);
}

/** Host keyboard while editing Display name. Returns true if consumed. */
export function beeperHandleText(b: Beeper, key: string): boolean {
  return pagerHandleText(b.pager, key);
}

export function beeperReset(b: Beeper, hal: Hal): void {
  identityReset(b.identity);
  contactsReset(b.contacts);
  pairReset(b.pair, hal);
  pageRadioReset(b.radio);
  pagerReset(b.pager);
  b.nvmReady = true;
  b.nvmHash = 0;
  storeSave(b, hal);
  hal.vibe(0);
}
