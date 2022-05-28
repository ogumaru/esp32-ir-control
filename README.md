# ESP32 IR Control

## これなに

ESP32 に HTTP で稼働する HTTP サーバにリクエストを送り、赤外線 LED からの信号で機器を操作するやつ

## 使用機器

| 部品                 | 名称(購入先リンク)                                                              |
| -------------------- | ------------------------------------------------------------------------------- |
| マイコンボード       | [WayinTop 社 ESP32 開発ボード](https://www.amazon.co.jp/gp/product/B086QKRY25/) |
| 赤外線 LED           | [KeeYees 社 HX1838](https://www.amazon.co.jp/gp/product/B07VMSDTR6/)            |
| 赤外線受光モジュール | [KeeYees 社 VS1838B](https://www.amazon.co.jp/gp/product/B07VMSDTR6/)           |

## 対応機器

### シーリングライト

- [ホタルクス社(NEC 社) HLDC06208 ](https://www.amazon.co.jp/gp/product/B07F1V1FMM/?th=1)
- [ホタルクス社(NEC 社) HLDZ06209 ](https://www.amazon.co.jp/gp/product/B07F1V7KF2/?th=1)

## 参考

[ESP32 で赤外線学習リモコンを作る](https://qiita.com/td2sk/items/4c0ef83bcc7e74e5e8d5)  
非同期処理、HTTP サーバ部分を参考にさせていただきました
